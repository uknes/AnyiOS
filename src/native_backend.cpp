#include <anyios/cpu_backend.hpp>
#include <anyios/native_a64.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace anyios::cpu {
namespace {
class NativeFixtureBackend final : public CpuBackend {
public:
    explicit NativeFixtureBackend(GuestMemory& memory) : memory_(memory) {}

    CpuState state() const override { return state_; }
    void set_state(const CpuState& state) override { state_ = state; }

    CpuEvent step() override {
        const auto first = memory_.fetch(state_.pc);
        if (!first) return {CpuEventKind::fault, 0, "native guest instruction fetch denied"};
        // Never dispatch arbitrary ARM64 instructions directly into the host.
        if (*first == 0xd4001001U) {
            return {CpuEventKind::unsupported, 0,
                    "native Darwin SVC requires a dedicated isolated trap handler"};
        }
        if (state_.pc > UINT64_MAX - 4) {
            return {CpuEventKind::fault, 0, "native guest program counter overflow"};
        }
        const auto second = memory_.fetch(state_.pc + 4);
        if (!second) return {CpuEventKind::fault, 0, "native guest return instruction denied"};
        if ((*first != 0xd2800540U && *first != 0x52800540U) ||
            *second != 0xd65f03c0U) {
            return {CpuEventKind::unsupported, 0,
                    "native backend executes only project-owned MOVZ #42/RET fixtures"};
        }
        std::array<std::byte, 8> code;
        for (unsigned i = 0; i < 4; ++i) {
            code[i] = std::byte((*first >> (i * 8)) & 0xff);
            code[i + 4] = std::byte((*second >> (i * 8)) & 0xff);
        }
        state_.x[0] = execute_owned_arm64_fixture(code);
        state_.pc = state_.x[30];
        return {};
    }

private:
    GuestMemory& memory_;
    CpuState state_;
};
}

std::unique_ptr<CpuBackend> make_native_fixture_backend(GuestMemory& memory) {
    return std::make_unique<NativeFixtureBackend>(memory);
}
}
