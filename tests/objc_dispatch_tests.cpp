#include <anyios/objc_dispatch.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;

void ensure(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}

// This portable CPU test verifies only the ABI-control-flow contract.
// Only pinned external Bitrise on Dynarmic proves real ARM64 IMP execution.
class GuardedMockCpu final : public anyios::cpu::CpuBackend {
public:
    GuardedMockCpu(std::uint64_t expected_receiver,
                   std::uint64_t expected_selector)
        : receiver_(expected_receiver), selector_(expected_selector) {
        value_.sp = 0x30000;
        value_.pc = 0x40000;
        value_.x[18] = 0xabcd;
    }
    anyios::cpu::CpuState state() const override { return value_; }
    void set_state(const anyios::cpu::CpuState& s) override { value_ = s; }
    anyios::cpu::CpuEvent step() override {
        if (value_.pc != 0x50000 || value_.x[0] != receiver_ ||
            value_.x[1] != selector_ || value_.x[2] || value_.x[3]) {
            return {anyios::cpu::CpuEventKind::unsupported, 0,
                    "unsafe synthetic callback arguments"};
        }
        value_.x[0] = 1;
        value_.pc = value_.x[30];
        return {anyios::cpu::CpuEventKind::stepped, 0, {}};
    }
private:
    anyios::cpu::CpuState value_{};
    std::uint64_t receiver_;
    std::uint64_t selector_;
};

void owned_guest_dispatch_regression() {
    GuestMemory memory(0x10000, 0x80000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    const auto rx = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::execute);
    ensure(memory.map(0x10000, 0x40000, rw) &&
           memory.map(0x50000, 0x4000, rx), "memory mapping failed");

    constexpr char name[] = "AppDelegate";
    constexpr char selector[] = "application:didFinishLaunchingWithOptions:";
    constexpr char encoding[] = "c32@0:8@16@24";
    constexpr std::array<std::byte, 4> ret{
        std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
    };
    ensure(memory.load(0x15000, std::span<const std::byte>(
               reinterpret_cast<const std::byte*>(name), sizeof(name))) &&
           memory.load(0x16000, std::span<const std::byte>(
               reinterpret_cast<const std::byte*>(selector), sizeof(selector))) &&
           memory.load(0x17000, std::span<const std::byte>(
               reinterpret_cast<const std::byte*>(encoding), sizeof(encoding))) &&
           memory.load(0x50000, ret) &&
           memory.write(0x18000, 0x20000, 8) &&
           memory.write(0x20000 + 8, 0, 8) &&
           memory.write(0x20000 + 32, 0x21000, 8) &&
           memory.write(0x21000 + 8, 32, 4) &&
           memory.write(0x21000 + 24, 0x15000, 8) &&
           memory.write(0x21000 + 32, 0x22000, 8) &&
           memory.write(0x22000, 24, 4) &&
           memory.write(0x22000 + 4, 1, 4) &&
           memory.write(0x22000 + 8, 0x16000, 8) &&
           memory.write(0x22000 + 16, 0x17000, 8) &&
           memory.write(0x22000 + 24, 0x50000, 8),
           "could not stage bounded compiler Objective-C metadata");

    anyios::macho::Image image;
    image.segments.push_back({"__TEXT", 0x100000000, 0x18000, 0, 0x18000, 2, 5, 5});
    image.sections.push_back({"__objc_classlist", "__DATA_CONST",
                              0x100008000, 8, 0x8000, 0, false, 0});
    image.sections.push_back({"__objc_const", "__DATA_CONST",
                              0x100011000, 0x2000, 0x11000, 0, false, 0});
    image.sections.push_back({"__cstring", "__TEXT",
                              0x100005000, 0x100, 0x5000, 0, false, 2});
    image.sections.push_back({"__objc_methname", "__TEXT",
                              0x100006000, 0x100, 0x6000, 0, false, 2});
    image.sections.push_back({"__objc_methtype", "__TEXT",
                              0x100007000, 0x100, 0x7000, 0, false, 2});

    anyios::darwin::ObjcIdentityProbe metadata(image, memory, 0x10000);
    anyios::darwin::GuestObjcClassRegistry classes(metadata, memory);
    anyios::darwin::GuestObjcSelectorRegistry selectors(metadata);
    anyios::darwin::GuestObjcObjectArena arena(memory, 0x60000, 0x4000);
    const auto guest = arena.allocate(metadata, 0x20000);
    ensure(guest.has_value(), "original class-owned guest allocation failed");
    GuardedMockCpu cpu(*guest, 0x16000);
    const auto initial = cpu.state();

    const auto result = anyios::darwin::invoke_owned_bool_launch(
        cpu, metadata, classes, selectors, arena,
        *guest, 0x16000, 0x400000, 32);
    ensure(result == 1 && cpu.state().pc == initial.pc &&
           cpu.state().x[18] == initial.x[18],
           "typed original guest IMP callback contract failed");
    ensure(!anyios::darwin::invoke_owned_bool_launch(
        cpu, metadata, classes, selectors, arena,
        *guest, 0x17000, 0x400000, 32),
           "unregistered selector pointer dispatched");
    ensure(!anyios::darwin::invoke_owned_bool_launch(
        cpu, metadata, classes, selectors, arena,
        *guest + 16, 0x16000, 0x400000, 32),
           "forged receiver dispatched");
    ensure(memory.write(*guest, 0x21000, 8),
           "could not stage forged isa");
    ensure(!anyios::darwin::invoke_owned_bool_launch(
        cpu, metadata, classes, selectors, arena,
        *guest, 0x16000, 0x400000, 32),
           "corrupt guest isa was accepted");
    ensure(memory.write(*guest, 0x20000, 8) && arena.release(*guest),
           "could not release original guest object");
    ensure(!anyios::darwin::invoke_owned_bool_launch(
        cpu, metadata, classes, selectors, arena,
        *guest, 0x16000, 0x400000, 32),
           "released guest receiver dispatched");
}
} // namespace

int main() {
    owned_guest_dispatch_regression();
    std::cout << "Bounded owned guest bool dispatch and refusal passed\n";
}
