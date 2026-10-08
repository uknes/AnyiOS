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
    test_fail_closed_untrusted_classlist();
    test_selector_bounds();
    std::cout << "Owned ObjC classlist/selector +class identity boundaries passed\n";
}
