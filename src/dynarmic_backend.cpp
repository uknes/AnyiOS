#include <anyios/cpu_backend.hpp>

#include <dynarmic/interface/A64/a64.h>
#include <dynarmic/interface/A64/config.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace anyios::cpu {
namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;

class Callbacks final : public Dynarmic::A64::UserCallbacks {
public:
    explicit Callbacks(GuestMemory& memory) : memory_(memory) {}

    std::optional<std::uint32_t> MemoryReadCode(Dynarmic::A64::VAddr address) override {
        const auto word = memory_.fetch(address);
        if (!word) fault_ = true;
        return word;
    }

    std::uint8_t MemoryRead8(Dynarmic::A64::VAddr addr) override {
        return static_cast<std::uint8_t>(load(addr, 1));
    }
    std::uint16_t MemoryRead16(Dynarmic::A64::VAddr addr) override {
        return static_cast<std::uint16_t>(load(addr, 2));
    }
    std::uint32_t MemoryRead32(Dynarmic::A64::VAddr addr) override {
        return static_cast<std::uint32_t>(load(addr, 4));
    }
    std::uint64_t MemoryRead64(Dynarmic::A64::VAddr addr) override {
        return load(addr, 8);
    }
    Dynarmic::A64::Vector MemoryRead128(Dynarmic::A64::VAddr addr) override {
        if (addr > UINT64_MAX - 8) {
            fault_ = true;
            return {0, 0};
        }
        return {load(addr, 8), load(addr + 8, 8)};
    }
    void MemoryWrite8(Dynarmic::A64::VAddr addr, std::uint8_t val) override {
        store(addr, val, 1);
    }
    void MemoryWrite16(Dynarmic::A64::VAddr addr, std::uint16_t val) override {
        store(addr, val, 2);
    }
    void MemoryWrite32(Dynarmic::A64::VAddr addr, std::uint32_t val) override {
        store(addr, val, 4);
    }
    void MemoryWrite64(Dynarmic::A64::VAddr addr, std::uint64_t val) override {
        store(addr, val, 8);
    }
    void MemoryWrite128(Dynarmic::A64::VAddr addr, Dynarmic::A64::Vector val) override {
        if (addr > UINT64_MAX - 15 || !memory_.allowed(addr, 16, Access::write)) {
            fault_ = true;
            return;
        }
        store(addr, val[0], 8);
        store(addr + 8, val[1], 8);
    }

    void InterpreterFallback(Dynarmic::A64::VAddr, std::size_t) override { fault_ = true; }
    void CallSVC(std::uint32_t immediate) override {
        if (trap_) fault_ = true;
        trap_ = immediate;
    }
    std::optional<std::uint32_t> take_trap() {
        auto result = trap_;
        trap_.reset();
        return result;
    }
    void ExceptionRaised(Dynarmic::A64::VAddr, Dynarmic::A64::Exception) override {
        fault_ = true;
    }
    void AddTicks(std::uint64_t ticks) override { ticks_ += ticks; }
    std::uint64_t GetTicksRemaining() override { return 1; }
    std::uint64_t GetCNTPCT() override { return ticks_; }
    bool failed() const { return fault_; }

private:
    std::uint64_t load(std::uint64_t addr, unsigned width) {
        const auto value = memory_.read(addr, width);
        if (!value) fault_ = true;
        return value.value_or(0);
    }
    void store(std::uint64_t addr, std::uint64_t value, unsigned width) {
        if (!memory_.write(addr, value, width)) fault_ = true;
    }
    GuestMemory& memory_;
    std::uint64_t ticks_ = 0;
    bool fault_ = false;
    std::optional<std::uint32_t> trap_;
};


Dynarmic::A64::UserConfig create_config(Callbacks* callbacks, const std::uint64_t* guest_tpidrro) {
    Dynarmic::A64::UserConfig config{};
    config.callbacks = callbacks;
    config.tpidrro_el0 = guest_tpidrro;
    config.enable_cycle_counting = false;
    return config;
}

class DynarmicBackend final : public CpuBackend {
public:
    explicit DynarmicBackend(GuestMemory& memory)
        : guest_memory_(memory), callbacks_(memory),
          config_(create_config(&callbacks_, &tpidrro_)), jit_(config_) {}

    CpuState state() const override {
        CpuState result;
        for (unsigned i = 0; i < 31; ++i) result.x[i] = jit_.GetRegister(i);
        result.pc = jit_.GetPC();
        result.sp = jit_.GetSP();
        result.pstate = jit_.GetPstate();
        result.guest_thread_id = thread_id_;
        result.tpidrro_el0 = tpidrro_;
        result.tpidrro_valid = tls_active_;
        return result;
    }

    void set_state(const CpuState& value) override {
        // A guest register must reference readable guest memory, not a host TLS address.
        if (value.tpidrro_valid &&
            (value.guest_thread_id == 0 || (value.tpidrro_el0 & 7u) != 0 ||
             !guest_memory_.allowed(value.tpidrro_el0, 8, Access::read))) {
            throw std::invalid_argument("invalid emulated Darwin thread TLS base");
        }
        thread_id_ = value.tpidrro_valid ? value.guest_thread_id : 0;
        tls_active_ = value.tpidrro_valid;
        tpidrro_ = value.tpidrro_valid ? value.tpidrro_el0 : 0;
        for (unsigned i = 0; i < 31; ++i) jit_.SetRegister(i, value.x[i]);
        jit_.SetPC(value.pc);
        jit_.SetSP(value.sp);
        jit_.SetPstate(value.pstate);
    }

    CpuEvent step() override {
        const auto instruction = guest_memory_.fetch(jit_.GetPC());
        if (instruction) {
            // Only read-only TPIDRRO_EL0 is authorized for an explicitly
            // selected guest thread. All other sysregs, including MSR, fail closed.
            const bool tpidrro_read = (*instruction & 0xffffffe0u) == 0xd53bd060u;
            const bool system_register = (*instruction & 0xfff00000u) == 0xd5300000u ||
                                         (*instruction & 0xfff00000u) == 0xd5100000u;
            if (system_register && (!tpidrro_read || !tls_active_)) {
                return {CpuEventKind::unsupported, 0,
                        "unconfigured or unsupported Darwin ARM64 system register"};
            }
        }
        static_cast<void>(jit_.Step());
        if (callbacks_.failed()) {
            return {CpuEventKind::fault, 0, "Dynarmic guest memory/CPU exception"};
        }
        if (auto immediate = callbacks_.take_trap()) {
            return {CpuEventKind::svc, *immediate, {}};
        }
        return {};
    }
private:
    GuestMemory& guest_memory_;
    Callbacks callbacks_;
    std::uint64_t tpidrro_ = 0;
    std::uint64_t thread_id_ = 0;
    bool tls_active_ = false;
    Dynarmic::A64::UserConfig config_;
    Dynarmic::A64::Jit jit_;
};
}

std::unique_ptr<CpuBackend> make_dynarmic_backend(GuestMemory& memory) {
    return std::make_unique<DynarmicBackend>(memory);
}
}
