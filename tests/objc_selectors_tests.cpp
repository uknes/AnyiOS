#include <anyios/objc_selectors.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

namespace {
void ensure(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void test_compiled_selector_identity() {
    using anyios::cpu::Access;
    anyios::cpu::GuestMemory memory(0x10000, 0x50000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    ensure(memory.map(0x10000, 0x40000, rw), "mapping failed");

    constexpr char selectors[] = "class\0viewDidLoad\0";
    constexpr char duplicate[] = "class";
    ensure(memory.load(0x14000, std::span<const std::byte>(
               reinterpret_cast<const std::byte*>(selectors), sizeof(selectors))) &&
           memory.load(0x14030, std::span<const std::byte>(
               reinterpret_cast<const std::byte*>(duplicate), sizeof(duplicate))),
           "could not stage compiled selector strings");

    anyios::macho::Image image;
    image.segments.push_back({"__TEXT", 0x100000000, 0x10000, 0, 0x10000, 2, 5, 5});
    image.sections.push_back({"__objc_methname", "__TEXT",
                              0x100004000, 0x80, 0x4000, 0, false, 2});
    const anyios::darwin::ObjcIdentityProbe metadata(image, memory, 0x10000);
    anyios::darwin::GuestObjcSelectorRegistry registry(metadata);

    ensure(registry.intern_compiled_selector(0x14000) == 0x14000,
           "first original guest selector was not canonical");
    ensure(registry.intern_compiled_selector(0x14030) == 0x14000,
           "duplicate compiler selector did not intern to original guest address");
    ensure(registry.intern_compiled_selector(0x14006) == 0x14006,
           "distinct guest selector collapsed to wrong identity");
    ensure(registry.size() == 2, "duplicate guest selector was inserted");

    ensure(!registry.intern_compiled_selector(0) &&
           !registry.intern_compiled_selector(UINT64_MAX) &&
           !registry.intern_compiled_selector(0x15000) &&
           !registry.intern_compiled_selector(0x1407f),
           "invalid, out-of-section or unmapped selector accepted");

    // A previously interned selector whose original guest memory changed
    // cannot be returned as a stale pointer or reinterpreted as a new SEL.
    ensure(memory.write(0x14000, 'x', 1), "could not mutate canonical selector");
    ensure(!registry.intern_compiled_selector(0x14030),
           "mutated canonical metadata reused as if still valid");
    ensure(registry.size() == 2, "stale canonical alias altered registry");
}

} // namespace

int main() {
    test_compiled_selector_identity();
    std::cout << "Bounded original guest ObjC selector alias identity and refusal passed\n";
}
