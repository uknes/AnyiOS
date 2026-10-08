#include <anyios/objc_registry.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
using anyios::cpu::GuestMemory;
using anyios::cpu::Access;
void ensure(bool yes, const char* why) {
    if (!yes) throw std::runtime_error(why);
}

GuestMemory memory_fixture(bool duplicate_names = false) {
    GuestMemory memory(0x10000, 0x60000);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    ensure(memory.map(0x10000, 0x40000, rw), "mapping failed");
    const char a[] = "AppDelegate";
    const char b[] = "ViewController";
    const auto first = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(a), sizeof(a));
    const auto second = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(b), sizeof(b));
    ensure(memory.load(0x15000, first) &&
           memory.load(0x15010, duplicate_names ? first : second) &&
           memory.write(0x18000, 0x20000, 8) &&
           memory.write(0x18008, 0x20100, 8) &&
           memory.write(0x20000 + 8, 0, 8) &&
           memory.write(0x20100 + 8, 0x20000, 8) &&
           memory.write(0x20000 + 32, 0x21000, 8) &&
           memory.write(0x20100 + 32, 0x21100, 8) &&
           memory.write(0x21000 + 8, 32, 4) &&
           memory.write(0x21100 + 8, 40, 4) &&
           memory.write(0x21000 + 24, 0x15000, 8) &&
           memory.write(0x21100 + 24, 0x15010, 8),
           "could not stage original-format ObjC class records");
    return memory;
}

anyios::macho::Image fixture_image() {
    anyios::macho::Image image;
    image.segments.push_back({"__TEXT", 0x100000000, 0x10000, 0, 0x10000, 2, 5, 5});
    image.sections.push_back({"__objc_classlist", "__DATA_CONST",
                              0x100008000, 16, 0x8000, 0, false, 0});
    image.sections.push_back({"__objc_const", "__DATA_CONST",
                              0x100011000, 0x2000, 0x11000, 0, false, 0});
    image.sections.push_back({"__cstring", "__TEXT",
                              0x100005000, 0x100, 0x5000, 0, false, 2});
    image.sections.push_back({"__objc_methname", "__TEXT",
                              0x100006000, 0x100, 0x6000, 0, false, 2});
    return image;
}

void check_registration_and_subclass() {
    auto memory = memory_fixture();
    const anyios::darwin::ObjcIdentityProbe classes(fixture_image(), memory, 0x10000);
    const anyios::darwin::GuestObjcClassRegistry registry(classes, memory);
    ensure(registry.size() == 2, "classes not registered");
    ensure(registry.find("AppDelegate") == 0x20000, "lookup failed");
    ensure(registry.find("ViewController") == 0x20100, "other class failed");
    ensure(registry.find_guest_name(0x15000) == 0x20000, "guest lookup failed");
    ensure(registry.find_guest_name(0x15010) == 0x20100, "other guest lookup failed");
    ensure(!registry.find("NSObject") && !registry.find_guest_name(0),
           "unknown or null class resolved");
    ensure(registry.local_superclass(0x20000) == 0, "root parent should be null");
    ensure(registry.local_superclass(0x20100) == 0x20000, "local parent not resolved");
    ensure(!registry.local_superclass(0x21000), "non-class accepted");

    // Malformed string, overlong and unmapped guest pointers must fail closed.
    ensure(!registry.find_guest_name(0x70000), "unmapped name accepted");
    ensure(!registry.find_guest_name(UINT64_MAX), "overflow name accepted");
    for (unsigned i = 0; i < 128; ++i)
        ensure(memory.write(0x18050 + i, 'A', 1), "string staging failed");
    ensure(!registry.find_guest_name(0x18050), "unterminated string accepted");
    ensure(memory.write(0x18050, 0x0a, 1), "control byte staging failed");
    ensure(!registry.find_guest_name(0x18050), "invalid name accepted");

    // The outside-framework superclass must not become a fictional local root.
    ensure(memory.write(0x20100 + 8, 0x50000, 8), "superclass mutation failed");
    const anyios::darwin::GuestObjcClassRegistry unresolved(classes, memory);
    ensure(!unresolved.local_superclass(0x20100), "unresolved framework parent accepted");
}

void check_objc_classname_section() {
    auto memory = memory_fixture();
    auto image = fixture_image();
    for (auto& section : image.sections) {
        if (section.name == "__cstring") {
            section.name = "__objc_classname";
        }
    }
    const anyios::darwin::ObjcIdentityProbe classes(image, memory, 0x10000);
    const anyios::darwin::GuestObjcClassRegistry registry(classes, memory);
    ensure(registry.find("AppDelegate") == 0x20000,
           "canonical __objc_classname class metadata not resolved");
    ensure(registry.find("ViewController") == 0x20100,
           "canonical __objc_classname secondary class metadata not resolved");
}

void check_unsupported_class_record_refused_without_losing_siblings() {
    auto memory = memory_fixture();
    // Refuse the bad secondary class without losing valid AppDelegate data.
    ensure(memory.write(0x21100 + 8, 0, 4),
           "cannot stage an unsupported instance size");
    const anyios::darwin::ObjcIdentityProbe classes(fixture_image(), memory, 0x10000);
    const anyios::darwin::GuestObjcClassRegistry registry(classes, memory);
    ensure(registry.size() == 1 && registry.unresolved_count() == 1,
           "unsupported record was registered or valid sibling was rejected");
    ensure(registry.find("AppDelegate") == 0x20000 &&
           !registry.find("ViewController"),
           "fail-closed registry leaked an unsupported Objective-C class");
    ensure(!registry.local_superclass(0x20100),
           "unresolved subclass incorrectly registered");
}


void check_bounded_guest_instance_method_inheritance() {
    auto memory = memory_fixture();
    const auto rx = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::execute);
    ensure(memory.map(0x50000, 0x4000, rx), "cannot map original guest IMP code");
    constexpr char selector[] = "handleEvent:";
    const std::array<std::byte, 4> ret{
        std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
    };
    ensure(memory.load(0x16000, std::span<const std::byte>(
               reinterpret_cast<const std::byte*>(selector), sizeof(selector))) &&
           memory.load(0x50000, ret) && memory.load(0x50004, ret) &&
           memory.write(0x21000 + 32, 0x21200, 8) &&
           memory.write(0x21200, 24, 4) &&
           memory.write(0x21200 + 4, 1, 4) &&
           memory.write(0x21200 + 8, 0x16000, 8) &&
           memory.write(0x21200 + 16, 0x16000, 8) &&
           memory.write(0x21200 + 24, 0x50000, 8),
           "cannot stage original-format guest method list");

    const anyios::darwin::ObjcIdentityProbe classes(fixture_image(), memory, 0x10000);
    const anyios::darwin::GuestObjcClassRegistry registry(classes, memory);
    const auto inherited = registry.resolve_local_instance_method(0x20100, "handleEvent:");
    ensure(inherited && inherited->selector == 0x16000 &&
           inherited->entry == 0x50000, "local superclass IMP was not inherited");
    ensure(!registry.resolve_local_instance_method(0x20100, "missing:") &&
           !registry.resolve_local_instance_method(0x24000, "handleEvent:") &&
           !registry.resolve_local_instance_method(0x20100, ""),
           "invalid selector or unregistered class dispatched");

    ensure(memory.write(0x21100 + 32, 0x21300, 8) &&
           memory.write(0x21300, 24, 4) &&
           memory.write(0x21300 + 4, 1, 4) &&
           memory.write(0x21300 + 8, 0x16000, 8) &&
           memory.write(0x21300 + 16, 0x16000, 8) &&
           memory.write(0x21300 + 24, 0x50004, 8),
           "cannot stage overridden guest method");
    const auto override_method =
        registry.resolve_local_instance_method(0x20100, "handleEvent:");
    ensure(override_method && override_method->entry == 0x50004,
           "subclass override did not take precedence");

    // Prevent accidental dispatch to an outside-framework superclass.
    ensure(memory.write(0x21100 + 32, 0, 8) &&
           memory.write(0x20100 + 8, 0x60000, 8),
           "cannot stage unresolved external parent");
    const anyios::darwin::GuestObjcClassRegistry external(classes, memory);
    ensure(!external.resolve_local_instance_method(0x20100, "handleEvent:"),
           "foreign superclass was traversed");

    // Malformed subclass metadata must NOT fall through to an inherited IMP.
    ensure(memory.write(0x20100 + 8, 0x20000, 8) &&
           memory.write(0x21100 + 32, 0x21300, 8) &&
           memory.write(0x21300, 0x80000018U, 4),
           "cannot stage unsupported subclass method list");
    const anyios::darwin::GuestObjcClassRegistry malformed(classes, memory);
    ensure(!malformed.resolve_local_instance_method(0x20100, "handleEvent:"),
           "unsupported overriding list incorrectly fell through to parent");
    ensure(memory.write(0x21100 + 32, 0, 8),
           "cannot clear unsupported subclass method list");

    // Cycles never recurse or fall through to a fabricated method.
    ensure(memory.write(0x20100 + 8, 0x20000, 8) &&
           memory.write(0x20000 + 8, 0x20100, 8),
           "cannot stage local inheritance cycle");
    const anyios::darwin::GuestObjcClassRegistry cycle(classes, memory);
    ensure(!cycle.resolve_local_instance_method(0x20100, "missing:"),
           "cyclic local inheritance was accepted");

    // An unsupported relative-method encoding cannot be guest-executed.
    ensure(memory.write(0x21200, 0x80000018U, 4),
           "cannot stage relative-method encoding");
    ensure(!cycle.resolve_local_instance_method(0x20000, "handleEvent:"),
           "unsupported relative Objective-C method metadata dispatched");
}

void check_duplicate_name_rejected() {
    auto memory = memory_fixture(true);
    const anyios::darwin::ObjcIdentityProbe classes(fixture_image(), memory, 0x10000);
    bool rejected = false;
    try {
        const anyios::darwin::GuestObjcClassRegistry invalid(classes, memory);
        static_cast<void>(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    ensure(rejected, "duplicate class registered");
}
} // namespace

int main() {
    check_registration_and_subclass();
    check_duplicate_name_rejected();
    check_bounded_guest_instance_method_inheritance();
    check_unsupported_class_record_refused_without_losing_siblings();
    check_objc_classname_section();
    std::cout << "Local Objective-C class registry, guest lookup, inheritance and refusal passed\n";
}
