#pragma once

#include <anyios/cpu_backend.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace anyios::abi {

enum class ScalarKind : std::uint8_t {
    signed8, unsigned8, signed16, unsigned16,
    signed32, unsigned32, signed64, unsigned64,
    pointer, aggregate
};

struct FixedSignature {
    std::vector<ScalarKind> parameters;
    bool variadic = false;
};

using HostFixedFunction = std::function<std::uint64_t(std::span<const std::uint64_t>)>;

std::vector<std::uint64_t> decode_fixed_arguments(
    const cpu::CpuState& guest, const FixedSignature& signature);

std::uint64_t call_fixed_host_function(
    const cpu::CpuState& guest, const FixedSignature& signature,
    const HostFixedFunction& host_function);

}
