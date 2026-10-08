#include <anyios/objc_objects.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;

void require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}

GuestMemory fixture_memory() {
    GuestMemory memory(0x10000, 0x80000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    require(memory.map(0x10000, 0x30000, rw), "cannot map fixture metadata");
    require(memory.write(0x18000, 0x20000, 8), "cannot install classlist");
    require(memory.write(0x20000 + 32, 0x21000, 8), "cannot install class data");
    require(memory.write(0x21000 + 4, 0, 4), "cannot install instanceStart");
    require(memory.write(0x21000 + 8, 32, 4), "cannot install instanceSize");
    return memory;
}

anyios::macho::Image fixture_image() {
    anyios::macho::Image image;
    image.segments.push_back({"__TEXT", 0x100000000, 0x10000, 0, 0x10000, 2, 5, 5});
    image.sections.push_back({"__objc_classlist", "__DATA_CONST",
                              0x100008000, 8, 0x8000, 0, false, 0});
    image.sections.push_back({"__objc_const", "__DATA",
                              0x100011000, 0x1000, 0x11000, 0, false, 0});
    return image;
}

void test_real_guest_instance_lifetime() {
    auto memory = fixture_memory();
    const anyios::darwin::ObjcIdentityProbe classes(fixture_image(), memory, 0x10000);
    anyios::darwin::GuestObjcObjectArena arena(memory, 0x50000, 0x4000);
    const auto object = arena.allocate(classes, 0x20000);
    require(object.has_value(), "valid class object allocation failed");
    require(*object == 0x50000 && (*object % 16) == 0, "object not aligned");
    require(memory.read(*object, 8) == 0x20000, "object isa is not the guest class");
    require(memory.read(*object + 8, 8) == 0, "object ivars not zeroed");
    require(memory.write(*object + 8, 0xCAFE, 8), "cannot write guest ivar");
    require(arena.is_live(*object) && arena.live_count() == 1, "object not tracked");
    require(arena.retain(*object), "valid retain failed");
    require(arena.release(*object), "first release failed");
    require(arena.is_live(*object) && memory.read(*object + 8, 8) == 0xCAFE,
            "object destroyed while retained");
    require(arena.release(*object), "last release failed");
    require(!arena.is_live(*object) && arena.live_count() == 0, "dead object still live");
    require(memory.read(*object, 8) == 0 && memory.read(*object + 8, 8) == 0,
            "deallocated object memory not cleared");
    require(!arena.retain(*object) && !arena.release(*object),
            "dangling pointer accepted after release");
    const auto next = arena.allocate(classes, 0x20000);
    require(next && *next != *object, "stale address reused for a fresh instance");
}

void test_bounds_and_invalid_classes() {
    auto memory = fixture_memory();
    const anyios::darwin::ObjcIdentityProbe classes(fixture_image(), memory, 0x10000);
    anyios::darwin::GuestObjcObjectArena arena(memory, 0x50000, 0x4000);
    require(!arena.allocate(classes, 0x21000), "non-class pointer allocated");
    require(!arena.retain(0x50000) && !arena.release(0x50000),
            "unallocated pointer accepted");
    require(memory.write(0x21000 + 8, 4, 4), "cannot write invalid instance size");
    require(!arena.allocate(classes, 0x20000), "undersized isa allocated");
    require(memory.write(0x21000 + 8, 4096, 4), "cannot write maximum instance size");
    for (unsigned i = 0; i < 4; ++i) {
        const auto value = arena.allocate(classes, 0x20000);
        require(value && *value == 0x50000 + 4096U * i,
                "unexpected bounded instance slot");
    }
    require(!arena.allocate(classes, 0x20000), "object arena capacity exceeded");
    require(!arena.retain(0x5ffff) && !arena.release(0x5ffff),
            "interior or unmapped object pointer accepted");
    bool overlap_rejected = false;
    try {
        anyios::darwin::GuestObjcObjectArena overlap(memory, 0x50000, 0x4000);
        static_cast<void>(overlap);
    } catch (const std::invalid_argument&) {
        overlap_rejected = true;
    }
    require(overlap_rejected, "duplicate object mapping accepted");
}

} // namespace

int main() {
    test_real_guest_instance_lifetime();
    test_bounds_and_invalid_classes();
    std::cout << "Bounded Objective-C guest instances, isa, retain/release and stale pointers passed\n";
}
