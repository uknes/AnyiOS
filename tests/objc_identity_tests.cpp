#include <anyios/objc_identity.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;

GuestMemory make_memory() {
    GuestMemory memory(0x10000, 0x40000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    if (!memory.map(0x10000, 0x30000, rw)) {
        throw std::runtime_error("ObjC metadata fixture mapping failed");
    }
    return memory;
}

anyios::macho::Image make_image() {
    anyios::macho::Image image;
    image.segments.push_back({"__TEXT", 0x100000000, 0x10000, 0, 0x10000, 2, 5, 5});
    image.sections.push_back({"__objc_methname", "__TEXT",
                              0x100004000, 22, 0x4000, 0, false, 2});
    image.sections.push_back({"__objc_classlist", "__DATA_CONST",
                              0x100008000, 8, 0x8000, 0, false, 0});
    image.sections.push_back({"__cstring", "__TEXT",
                              0x100005000, 32, 0x5000, 0, false, 2});
    image.sections.push_back({"__objc_const", "__DATA",
                              0x100011000, 0x1200, 0x11000, 0, false, 0});
    return image;
}

void test_valid_class_identity() {
    auto memory = make_memory();
    const char names[] = "class\0viewDidLoad\0\0";
    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(names), sizeof(names));
    if (!memory.load(0x14000, bytes) ||
        !memory.write(0x18000, 0x20000, 8) ||
        !memory.write(0x20000, 0x23000, 8)) {
        throw std::runtime_error("owned ObjC metadata test write failed");
    }
    const anyios::darwin::ObjcIdentityProbe probe(make_image(), memory, 0x10000);
    assert(probe.is_local_class(0x20000));
    assert(!probe.is_local_class(0x23000));
    assert(probe.selector_name(0x14000) == "class");
    assert(probe.selector_name(0x14006) == "viewDidLoad");
    assert(probe.invoke_class_identity(0x20000, 0x14000) == 0x20000);
    assert(!probe.invoke_class_identity(0x20000, 0x14006));
    assert(!probe.invoke_class_identity(0x23000, 0x14000));
    assert(!probe.invoke_class_identity(0x20000, 0x10000));
}

void test_clang_guest_method_lookup() {
    auto memory = make_memory();
    constexpr auto rx = anyios::cpu::bits(Access::read) |
                        anyios::cpu::bits(Access::execute);
    if (!memory.map(0x40000, 0x1000, rx)) {
        throw std::runtime_error("cannot map owned guest ObjC IMP");
    }
    const char selectors[] = "class\0viewDidLoad\0\0";
    const char class_name[] = "AppDelegate\0";
    const std::array<std::byte, 4> ret_instruction{
        std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
    };
    if (!memory.load(0x14000, std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(selectors), sizeof(selectors))) ||
        !memory.load(0x15000, std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(class_name), sizeof(class_name))) ||
        !memory.write(0x18000, 0x20000, 8) ||
        !memory.write(0x20000 + 32, 0x21000, 8) ||
        !memory.write(0x21000 + 4, 0, 4) ||
        !memory.write(0x21000 + 8, 32, 4) ||
        !memory.write(0x21000 + 24, 0x15000, 8) ||
        !memory.write(0x21000 + 32, 0x22000, 8) ||
        !memory.write(0x22000, 24, 4) ||
        !memory.write(0x22000 + 4, 1, 4) ||
        !memory.write(0x22000 + 8, 0x14006, 8) ||
        !memory.write(0x22000 + 16, 0x14006, 8) ||
        !memory.write(0x22000 + 24, 0x40000, 8) ||
        !memory.load(0x40000, ret_instruction)) {
        throw std::runtime_error("cannot stage owned Clang ObjC class/method metadata");
    }
    const anyios::darwin::ObjcIdentityProbe probe(make_image(), memory, 0x10000);
    if (probe.local_class_name(0x20000) != "AppDelegate") {
        throw std::runtime_error("Clang guest class name not resolved");
    }
    const auto method = probe.local_instance_method(0x20000, "viewDidLoad");
    if (!method || method->selector != 0x14006 || method->entry != 0x40000) {
        throw std::runtime_error("Clang guest class method implementation not resolved");
    }
    if (probe.local_instance_method(0x20000, "doesNotExist") ||
        probe.local_instance_method(0x24000, "viewDidLoad")) {
        throw std::runtime_error("unsupported guest ObjC method dispatched");
    }
    if (!memory.write(0x22000, 0x80000018U, 4)) {
        throw std::runtime_error("cannot mutate guest method table encoding");
    }
    if (probe.local_instance_method(0x20000, "viewDidLoad")) {
        throw std::runtime_error("unsupported relative ObjC methods accepted");
    }
}

void test_fail_closed_untrusted_classlist() {
    auto memory = make_memory();
    if (!memory.write(0x18000, 0x50000, 8)) {
        throw std::runtime_error("cannot stage invalid ObjC class pointer");
    }
    try {
        const anyios::darwin::ObjcIdentityProbe invalid(make_image(), memory, 0x10000);
        static_cast<void>(invalid);
        throw std::runtime_error("unmapped classlist incorrectly accepted");
    } catch (const std::invalid_argument&) { }
    if (!memory.write(0x18000, 0x20000, 8)) {
        throw std::runtime_error("cannot stage valid ObjC pointer");
    }
    auto image = make_image();
    image.sections[1].size = 9;
    try {
        const anyios::darwin::ObjcIdentityProbe invalid(image, memory, 0x10000);
        static_cast<void>(invalid);
        throw std::runtime_error("misaligned classlist incorrectly accepted");
    } catch (const std::invalid_argument&) { }
}

void test_selector_bounds() {
    auto memory = make_memory();
    if (!memory.write(0x18000, 0x20000, 8)) {
        throw std::runtime_error("cannot stage ObjC pointer");
    }
    std::array<std::byte, 22> no_nul{};
    no_nul.fill(std::byte{65});
    if (!memory.load(0x14000, no_nul)) {
        throw std::runtime_error("cannot stage ObjC selector string");
    }
    const anyios::darwin::ObjcIdentityProbe probe(make_image(), memory, 0x10000);
    assert(!probe.selector_name(0x14000));
}

} // namespace

int main() {
    test_valid_class_identity();
    test_clang_guest_method_lookup();
    test_fail_closed_untrusted_classlist();
    test_selector_bounds();
    std::cout << "Owned ObjC classlist/selector +class identity boundaries passed\n";
}
