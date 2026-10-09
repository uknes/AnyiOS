#include <anyios/objc_dispatch.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool valid, const char* why) {
    if (!valid) throw std::runtime_error(why);
}

class ObservingGuestBackend final : public anyios::cpu::CpuBackend {
public:
    std::uint64_t expected_receiver = 0;
    std::uint64_t expected_selector = 0;
    unsigned calls = 0;
    bool corrupt_x18 = false;

    anyios::cpu::CpuState state() const override { return current_; }
    void set_state(const anyios::cpu::CpuState& value) override { current_ = value; }
    anyios::cpu::CpuEvent step() override {
        check(current_.pc == 0x50000, "wrong original guest IMP");
        check(current_.x[0] == expected_receiver &&
              current_.x[1] == expected_selector &&
              current_.x[2] == 0 && current_.x[3] == 0,
              "incorrect Apple ARM64 self/cmd/object argument registers");
        ++calls;
        current_.x[0] = 1;
        if (corrupt_x18) current_.x[18] ^= 1;
        current_.pc = current_.x[30];
        return {};
    }
private:
    anyios::cpu::CpuState current_{};
};

void test_owned_original_method_dispatch_contract() {
    using anyios::cpu::Access;
    anyios::cpu::GuestMemory memory(0x10000, 0x80000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    const auto rx = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::execute);
    check(memory.map(0x10000, 0x40000, rw) &&
          memory.map(0x50000, 0x4000, rx), "guest memory mapping failed");

    constexpr char selector[] = "application:didFinishLaunchingWithOptions:";
    constexpr char classname[] = "AppDelegate";
    constexpr char types[] = "c32@0:8@16@24";
    constexpr std::array<std::byte, 4> ret{
        std::byte{0xc0}, std::byte{0x03},
        std::byte{0x5f}, std::byte{0xd6}
    };
    auto bytes = [](const auto& array) {
        return std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(array), sizeof(array));
    };
    check(memory.load(0x14000, bytes(selector)) &&
          memory.load(0x15000, bytes(classname)) &&
          memory.load(0x16000, bytes(types)) &&
          memory.load(0x50000, ret) &&
          memory.write(0x18000, 0x20000, 8) &&
          memory.write(0x20000 + 8, 0, 8) &&
          memory.write(0x20000 + 32, 0x21000, 8) &&
          memory.write(0x21000 + 4, 0, 4) &&
          memory.write(0x21000 + 8, 32, 4) &&
          memory.write(0x21000 + 24, 0x15000, 8) &&
          memory.write(0x21000 + 32, 0x22000, 8) &&
          memory.write(0x22000, 24, 4) &&
          memory.write(0x22000 + 4, 1, 4) &&
          memory.write(0x22000 + 8, 0x14000, 8) &&
          memory.write(0x22000 + 16, 0x16000, 8) &&
          memory.write(0x22000 + 24, 0x50000, 8),
          "compiler-style ObjC2 fixture setup failed");
    anyios::macho::Image image;
    image.segments.push_back({"__TEXT", 0x100000000, 0x10000,
                              0, 0x10000, 2, 5, 5});
    image.sections.push_back({"__objc_methname", "__TEXT",
                              0x100004000, 0x100, 0x4000, 0, false, 2});
    image.sections.push_back({"__cstring", "__TEXT",
                              0x100005000, 0x100, 0x5000, 0, false, 2});
    image.sections.push_back({"__objc_methtype", "__TEXT",
                              0x100006000, 0x100, 0x6000, 0, false, 2});
    image.sections.push_back({"__objc_classlist", "__DATA",
                              0x100008000, 8, 0x8000, 0, false, 0});
    image.sections.push_back({"__objc_const", "__DATA",
                              0x100011000, 0x2000, 0x11000, 0, false, 0});

    const anyios::darwin::ObjcIdentityProbe meta(image, memory, 0x10000);
    const anyios::darwin::GuestObjcClassRegistry classes(meta, memory);
    anyios::darwin::GuestObjcSelectorRegistry selectors(meta);
    anyios::darwin::GuestObjcObjectArena objects(memory, 0x60000, 0x4000);
    const auto instance = objects.allocate(meta, 0x20000);
    check(instance.has_value(), "could not allocate original guest instance");
    ObservingGuestBackend backend;
    anyios::cpu::CpuState before{};
    before.sp = 0x30000;
    before.pc = 0x40000;
    before.x[18] = 0x1234;
    before.x[19] = 0x5678;
    backend.set_state(before);
    backend.expected_receiver = *instance;
    backend.expected_selector = 0x14000;

    const auto send = [&](std::uint64_t object, std::uint64_t sel,
                          std::uint64_t app = 0, std::uint64_t options = 0) {
        return anyios::darwin::invoke_guest_bool_launch_message(
            backend, memory, meta, classes, selectors, objects,
            object, sel, app, options, 0x400000, 8);
    };
    check(send(*instance, 0x14000) == 1 && backend.calls == 1,
          "validated guest ARM64 BOOL message not dispatched");
    check(backend.state().pc == before.pc &&
          backend.state().sp == before.sp &&
          backend.state().x[18] == before.x[18] &&
          backend.state().x[19] == before.x[19],
          "callback leaked guest register changes");
    check(!send(0x20000, 0x14000) && !send(*instance, UINT64_MAX) &&
          !send(*instance, 0x14000, 0xdeadbeef) &&
          !send(*instance, 0x14000, 0, 0xdeadbeef) &&
          backend.calls == 1,
          "invalid receiver, selector or foreign object arguments dispatched");
    backend.corrupt_x18 = true;
    bool refused_clobber = false;
    try {
        (void)send(*instance, 0x14000);
    } catch (const std::runtime_error&) {
        refused_clobber = true;
    }
    check(refused_clobber &&
          backend.state().x[18] == before.x[18] &&
          backend.state().pc == before.pc,
          "platform register clobber was not refused and restored");
    backend.corrupt_x18 = false;
    const auto before_rejection = backend.calls;
    check(memory.write(0x16000, 'v', 1), "cannot mutate method type");
    check(!send(*instance, 0x14000) && backend.calls == before_rejection,
          "unknown method type dispatched");
    check(memory.write(0x16000, 'c', 1) &&
          memory.write(*instance, 0x22220, 8),
          "cannot mutate original guest isa");
    check(!send(*instance, 0x14000) && backend.calls == before_rejection,
          "unregistered guest isa dispatched");
    check(memory.write(*instance, 0x20000, 8) &&
          objects.release(*instance) && !objects.is_live(*instance),
          "guest instance retirement failed");
    check(!send(*instance, 0x14000) && backend.calls == before_rejection,
          "use-after-release instance dispatched");
}

} // namespace

int main() {
    test_owned_original_method_dispatch_contract();
    std::cout << "Narrow Objective-C guest instance BOOL message ABI tests passed\n";
}
