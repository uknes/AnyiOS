#pragma once

#include <anyios/guest_memory.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace anyios::cpu {

struct CpuState {
    std::array<std::uint64_t, 31> x{};
    std::uint64_t pc = 0;
    std::uint64_t sp = 0;
    std::uint32_t pstate = 0;
    // Explicitly selected emulated Darwin thread; never a host TLS pointer.
    std::uint64_t guest_thread_id = 0;
    std::uint64_t tpidrro_el0 = 0;
    bool tpidrro_valid = false;
};

enum class CpuEventKind {
    stepped,
    svc,
    fault,
    unsupported,
    returned
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
    CpuEvent run_until_event(std::uint64_t max_steps,
                             std::optional<std::uint64_t> return_pc = std::nullopt) {
        if (max_steps == 0) {
            return {CpuEventKind::unsupported, 0, "guest instruction budget exhausted"};
        }
        for (std::uint64_t i = 0; i < max_steps; ++i) {
            const auto event = step();
            if (event.kind != CpuEventKind::stepped) return event;
            if (return_pc && state().pc == *return_pc) {
                return {CpuEventKind::returned, 0, {}};
            }
        }
        return {CpuEventKind::unsupported, 0, "guest instruction budget exhausted"};
    }
};

std::unique_ptr<CpuBackend> make_dynarmic_backend(GuestMemory& memory);
std::unique_ptr<CpuBackend> make_native_fixture_backend(GuestMemory& memory);

}
