#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Bytes = std::vector<std::byte>;
using anyios::cpu::GuestMemory;
using anyios::cpu::Access;

void u16(Bytes& b, std::size_t at, std::uint16_t v) {
    for (unsigned i = 0; i < 2; ++i) b.at(at + i) = std::byte((v >> (8 * i)) & 0xff);
}
void u32(Bytes& b, std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) b.at(at + i) = std::byte((v >> (8 * i)) & 0xff);
}
void u64(Bytes& b, std::size_t at, std::uint64_t v) {
    u32(b, at, static_cast<std::uint32_t>(v));
    u32(b, at + 4, static_cast<std::uint32_t>(v >> 32));
}
void check(bool good, std::string_view reason) {
    if (!good) throw std::runtime_error(std::string(reason));
}

Bytes fixture() {
    Bytes b(8192);
    u32(b, 0, 0xfeedfacf);
    u32(b, 4, 0x0100000c);
    u32(b, 12, 2);
    u32(b, 16, 5);
    u32(b, 20, 208);

    u32(b, 32, 0x19); u32(b, 36, 72);
    constexpr char text[] = "__TEXT";
    for (std::size_t i = 0; i < sizeof(text)-1; ++i) b.at(40+i) = std::byte(text[i]);
    u64(b, 32+24, 0x100000000ULL); u64(b, 32+32, 4096);
    u64(b, 32+40, 0); u64(b, 32+48, 4096);
    u32(b, 32+56, 5); u32(b, 32+60, 5);

    u32(b, 104, 0x19); u32(b, 108, 72);
    constexpr char data[] = "__DATA";
    for (std::size_t i = 0; i < sizeof(data)-1; ++i) b.at(112+i) = std::byte(data[i]);
    u64(b, 104+24, 0x100001000ULL); u64(b, 104+32, 4096);
    u64(b, 104+40, 4096); u64(b, 104+48, 4096);
    u32(b, 104+56, 3); u32(b, 104+60, 3);

    u32(b, 176, 0x80000028); u32(b, 180, 24); u64(b, 184, 0x300);
    u32(b, 200, 0x32); u32(b, 204, 24);
    u32(b, 208, 2);
    u32(b, 224, 0x80000034); u32(b, 228, 16);
    u32(b, 232, 256); u32(b, 236, 80);

    constexpr std::size_t payload = 256;
    u32(b, payload+4, 28); u32(b, payload+8, 64);
    u32(b, payload+12, 68); u32(b, payload+16, 1);
    u32(b, payload+20, 1);
    u32(b, payload+28, 2); u32(b, payload+36, 12);
    u32(b, payload+40, 24);
    u16(b, payload+44, 4096); u16(b, payload+46, 6);
    u64(b, payload+48, 4096);
    u16(b, payload+60, 1); u16(b, payload+62, 0);
    u32(b, payload+64, 1);
    b[payload+68] = std::byte{'f'};
    b[payload+69] = std::byte{'o'};
    b[payload+70] = std::byte{'o'};
    u64(b, 4096, (std::uint64_t{2} << 51) | 0x1234);
    u64(b, 4104, (std::uint64_t{1} << 63) | (std::uint64_t{3} << 24));
    u32(b, 0x300, 0xd2800540);
    u32(b, 0x304, 0xd65f03c0);
    return b;
}

void throws(const Bytes& source, GuestMemory& memory,
            std::uint64_t base,
            std::array<std::uint64_t, 1> imports,
            std::string_view error) {
    try {
        static_cast<void>(anyios::loader::stage_linked_image(source, memory, base, imports));
    } catch (const anyios::macho::FormatError& caught) {
        check(std::string_view(caught.what()).find(error) != std::string_view::npos, caught.what());
        return;
    }
    throw std::runtime_error("invalid linked image accepted");
}

void run() {
    const auto original = fixture();
    const std::array<std::uint64_t,1> imports{0x14000};
    GuestMemory memory(0x10000, 0x20000);
    const auto result = anyios::loader::stage_linked_image(original, memory, 0x10000, imports);
    check(result.guest_base == 0x10000 && result.guest_entry == 0x10300 &&
          result.segment_count == 2 && result.patched_pointers == 2, "linked image result");
    check(memory.fetch(0x10300) == 0xd2800540 &&
          memory.fetch(0x10304) == 0xd65f03c0, "linked executable instructions");
    check(memory.read(0x11000,8) == 0x11234, "rebase should use relocated guest base");
    check(memory.read(0x11008,8) == 0x14003, "bind target should preserve resolved guest address");
    check(!memory.write(0x10300, 0, 4), "executable page writable");
    check(!memory.fetch(0x11000), "data page executable");

    {
        GuestMemory guarded(0x10000, 0x20000);
        check(guarded.map(0x11000, 4096, 3), "guard setup");
        throws(original, guarded, 0x10000, imports, "linked guest mapping failed");
        check(!guarded.fetch(0x10300).has_value(), "failed load leaked code mapping");
        check(guarded.write(0x11000, 0x11223344, 4), "failed load modified guard");
    }
    {
        GuestMemory narrow(0x10000, 0x20000);
        throws(original, narrow, 0x2f000, imports, "linked guest mapping failed");
        check(!narrow.fetch(0x2f000), "failed load leaked partial mappings");
    }
    {
        auto bad = original;
        u16(bad, 256+46, 9);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "unsupported chained pointer format");
        check(!fresh.fetch(0x10300), "invalid fixup leaked mapping");
    }
    {
        auto bad = original;
        u32(bad, 104+60, 7);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "unsupported linked segment protections");
    }
    {
        auto bad = original;
        u32(bad, 224, 0x80000022);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "unsupported linked image legacy dyld");
    }
    {
        GuestMemory fresh(0x10000, 0x20000);
        throws(original, fresh, 0x10000, {0}, "unresolved chained bind ordinal");
    }
    {
        auto bad = original;
        u64(bad, 4096, 0xffee);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "linked rebase target outside image");
    }
}

}
int main() {
    try {
        run();
        std::cout << "Linked-image guest mapping and relocatable dyld patch tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "linked-image test failed: " << error.what() << '\n';
        return 1;
    }
}
