#pragma once

#include <anyios/guest_memory.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace anyios::cpu {

struct CpuState {
    std::array<std::uint64_t, 31> x{};
    std::uint64_t pc = 0;
    std::uint64_t sp = 0;
    std::uint32_t pstate = 0;
};

enum class CpuEventKind {
    stepped,
    svc,
    fault,
    unsupported
};

struct CpuEvent {
    CpuEventKind kind = CpuEventKind::stepped;
    std::uint32_t svc_immediate = 0;
    std::string diagnostic;
};

class CpuBackend {
public:
    virtual ~CpuBackend() = default;
    virtual CpuState state() const = 0;
    virtual void set_state(const CpuState& state) = 0;
    virtual CpuEvent step() = 0;
};

std::unique_ptr<CpuBackend> make_dynarmic_backend(GuestMemory& memory);
std::unique_ptr<CpuBackend> make_native_fixture_backend(GuestMemory& memory);

}
