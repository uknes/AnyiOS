#include <anyios/abi_thunk.hpp>

#include <cstdint>
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
