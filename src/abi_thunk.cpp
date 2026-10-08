#include <anyios/abi_thunk.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace anyios::abi {
namespace {
template<class Signed>
std::uint64_t signed_argument(std::uint64_t raw) {
    static_assert(std::is_signed_v<Signed>);
    using Unsigned = std::make_unsigned_t<Signed>;
    const auto masked = static_cast<Unsigned>(raw);
    const auto interpreted = static_cast<Signed>(masked);
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(interpreted));
}
}

std::vector<std::uint64_t> decode_fixed_arguments(
    const cpu::CpuState& guest, const FixedSignature& signature) {
    if (signature.variadic) {
        throw std::invalid_argument("Apple ARM64 variadic arguments require a dedicated ABI bridge");
    }
    if (signature.parameters.size() > 8) {
        throw std::invalid_argument("Apple ARM64 stack/aggregate argument support is not implemented");
    }
    std::vector<std::uint64_t> values;
    values.reserve(signature.parameters.size());
    for (std::size_t i = 0; i < signature.parameters.size(); ++i) {
        const auto raw = guest.x[i];
        switch (signature.parameters[i]) {
        case ScalarKind::signed8:
            values.push_back(signed_argument<std::int8_t>(raw)); break;
        case ScalarKind::unsigned8:
            values.push_back(static_cast<std::uint8_t>(raw)); break;
        case ScalarKind::signed16:
            values.push_back(signed_argument<std::int16_t>(raw)); break;
        case ScalarKind::unsigned16:
            values.push_back(static_cast<std::uint16_t>(raw)); break;
        case ScalarKind::signed32:
            values.push_back(signed_argument<std::int32_t>(raw)); break;
        case ScalarKind::unsigned32:
            values.push_back(static_cast<std::uint32_t>(raw)); break;
        case ScalarKind::signed64:
        case ScalarKind::unsigned64:
            values.push_back(raw); break;
        case ScalarKind::pointer:
            throw std::invalid_argument("raw guest pointers cannot cross a host ABI thunk");
        case ScalarKind::aggregate:
            throw std::invalid_argument("aggregate arguments need a signature-specific ABI bridge");
        default:
            throw std::invalid_argument("unsupported Apple ARM64 scalar argument kind");
        }
    }
    return values;
}

std::vector<std::uint64_t> decode_apple_variadic_arguments(
    const cpu::CpuState& guest, const cpu::GuestMemory& memory,
    const FixedSignature& fixed, std::span<const ScalarKind> variadic_types) {
    if (!fixed.variadic || (guest.sp & 15U) != 0) {
        throw std::invalid_argument("Apple variadic call requires aligned SP and explicit signature");
    }
    auto fixed_only = fixed;
    fixed_only.variadic = false;
    auto values = decode_fixed_arguments(guest, fixed_only);
    if (variadic_types.size() > 16) {
        throw std::invalid_argument("Apple ARM64 variadic argument limit exceeded");
    }
    for (std::size_t index = 0; index < variadic_types.size(); ++index) {
        const auto type = variadic_types[index];
        if (type != ScalarKind::signed32 && type != ScalarKind::unsigned32 &&
            type != ScalarKind::signed64 && type != ScalarKind::unsigned64) {
            throw std::invalid_argument("unsupported Apple vararg type: use promoted int or long");
        }
        const auto displacement = static_cast<std::uint64_t>(index) * 8;
        if (displacement > std::numeric_limits<std::uint64_t>::max() - guest.sp) {
            throw std::invalid_argument("Apple ARM64 variadic stack pointer overflow");
        }
        const auto slot = memory.read(guest.sp + displacement, 8);
        if (!slot) throw std::invalid_argument("Apple ARM64 variadic stack slot inaccessible");
        switch (type) {
            case ScalarKind::signed32:
                values.push_back(signed_argument<std::int32_t>(*slot)); break;
            case ScalarKind::unsigned32:
                values.push_back(static_cast<std::uint32_t>(*slot)); break;
            default: values.push_back(*slot); break;
        }
    }
    return values;
}

std::uint64_t call_variadic_host_function(
    const cpu::CpuState& guest, const cpu::GuestMemory& memory,
    const FixedSignature& fixed, std::span<const ScalarKind> variadic_types,
    const HostFixedFunction& host_function) {
    if (!host_function) throw std::invalid_argument("variadic thunk host callback missing");
    const auto values = decode_apple_variadic_arguments(guest, memory, fixed, variadic_types);
    return host_function(values);
}

std::uint64_t invoke_guest_callback(
    cpu::CpuBackend& backend, std::uint64_t entry,
    std::span<const std::uint64_t> arguments,
    std::uint64_t return_pc, std::uint64_t instruction_budget) {
    if (arguments.size() > 8 || entry == return_pc || instruction_budget == 0) {
        throw std::invalid_argument("invalid bounded guest callback invocation");
    }
    const auto saved = backend.state();
    if ((saved.sp & 15U) != 0 || saved.sp == 0) {
        throw std::invalid_argument("guest callback stack not 16-byte aligned");
    }
    auto called = saved;
    called.pc = entry;
    called.x[30] = return_pc;
    for (std::size_t i = 0; i < arguments.size(); ++i) called.x[i] = arguments[i];
    backend.set_state(called);
    try {
        const auto event = backend.run_until_event(instruction_budget, return_pc);
        const auto result = backend.state();
        if (event.kind != cpu::CpuEventKind::returned) {
            throw std::runtime_error("guest callback stopped without returning: " + event.diagnostic);
        }
        if (result.x[18] != saved.x[18]) {
            throw std::runtime_error("guest callback clobbered platform register x18");
        }
        for (unsigned i = 19; i <= 29; ++i) {
            if (result.x[i] != saved.x[i]) {
                throw std::runtime_error("guest callback clobbered callee-saved x19-x29");
            }
        }
        if (result.sp != saved.sp) {
            throw std::runtime_error("guest callback corrupted stack pointer");
        }
        const auto value = result.x[0];
        backend.set_state(saved);
        return value;
    } catch (...) {
        backend.set_state(saved);
        throw;
    }
}

std::uint64_t call_fixed_host_function(
    const cpu::CpuState& guest, const FixedSignature& signature,
    const HostFixedFunction& host_function) {
    if (!host_function) {
        throw std::invalid_argument("host ABI thunk has no registered implementation");
    }
    const auto values = decode_fixed_arguments(guest, signature);
    return host_function(values);
}
}
