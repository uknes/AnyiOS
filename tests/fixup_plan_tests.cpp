#include <anyios/fixup_plan.hpp>

#include <algorithm>
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

void put16(Bytes& b, std::size_t at, std::uint16_t value) {
    for (unsigned i = 0; i < 2; ++i) b.at(at + i) = std::byte((value >> (8 * i)) & 255);
}
void put32(Bytes& b, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) b.at(at + i) = std::byte((value >> (8 * i)) & 255);
}
void put64(Bytes& b, std::size_t at, std::uint64_t value) {
    put32(b, at, static_cast<std::uint32_t>(value));
    put32(b, at + 4, static_cast<std::uint32_t>(value >> 32));
}
std::uint64_t get64(const Bytes& b, std::size_t at) {
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) {
        result |= std::uint64_t(std::to_integer<std::uint8_t>(b.at(at + i))) << (8 * i);
    }
    return result;
}

Bytes image() {
    Bytes b(8192);
    put32(b, 0, 0xfeedfacf);
    put32(b, 4, 0x0100000c);
    put32(b, 12, 2);
    put32(b, 16, 3);
    put32(b, 20, 160);

    put32(b, 32, 0x19);
    put32(b, 36, 72);
    constexpr char text[] = "__TEXT";
    for (std::size_t i = 0; i < sizeof(text) - 1; ++i) b[40 + i] = std::byte(text[i]);
    put64(b, 32 + 24, 0x10000);
    put64(b, 32 + 32, 4096);
    put64(b, 32 + 40, 0);
    put64(b, 32 + 48, 4096);
    put32(b, 32 + 56, 5);
    put32(b, 32 + 60, 5);

    put32(b, 104, 0x19);
    put32(b, 108, 72);
    constexpr char data[] = "__DATA";
    for (std::size_t i = 0; i < sizeof(data) - 1; ++i) b[112 + i] = std::byte(data[i]);
    put64(b, 104 + 24, 0x11000);
    put64(b, 104 + 32, 4096);
    put64(b, 104 + 40, 4096);
    put64(b, 104 + 48, 4096);
    put32(b, 104 + 56, 3);
    put32(b, 104 + 60, 3);

    put32(b, 176, 0x80000034);
    put32(b, 180, 16);
    put32(b, 184, 256);
    put32(b, 188, 80);

    constexpr std::size_t payload = 256;
    put32(b, payload + 4, 28);
    put32(b, payload + 8, 64);
    put32(b, payload + 12, 68);
    put32(b, payload + 16, 1);
    put32(b, payload + 20, 1);
    put32(b, payload + 28, 2);
    put32(b, payload + 36, 12);

    put32(b, payload + 40, 24);
    put16(b, payload + 44, 4096);
    put16(b, payload + 46, 6);
    put64(b, payload + 48, 4096);
    put16(b, payload + 60, 1);
    put16(b, payload + 62, 0);
    put32(b, payload + 64, 1);
    b[payload + 68] = std::byte{'f'};
    b[payload + 69] = std::byte{'o'};
    b[payload + 70] = std::byte{'o'};
    put64(b, 4096, (std::uint64_t{2} << 51) | 0x1234);
    put64(b, 4104, (std::uint64_t{1} << 63) | (std::uint64_t{3} << 24));
    return b;
}

void check(bool yes, std::string_view message) {
    if (!yes) throw std::runtime_error(std::string(message));
}
void rejects(const Bytes& b, std::array<std::uint64_t, 1> resolved,
             std::string_view expected) {
    try {
        const auto parsed = anyios::macho::inspect(b);
        (void)anyios::dyld::plan_chained_fixups(b, parsed, resolved);
    } catch (const anyios::macho::FormatError& error) {
        check(std::string_view(error.what()).find(expected) != std::string_view::npos,
              std::string("unexpected rejection: ") + error.what());
        return;
    }
    throw std::runtime_error("malformed chained fixup was accepted");
}

void run() {
    auto b = image();
    const std::array<std::uint64_t, 1> targets{0x40000};
    const auto parsed = anyios::macho::inspect(b);
    check(parsed.has_chained_fixups && parsed.chained_fixups_range.has_value(), "missing chain");
    check(parsed.chained_imports.size() == 1 && parsed.chained_imports[0] == "foo", "import name");
    const auto patches = anyios::dyld::plan_chained_fixups(b, parsed, targets);
    check(patches.size() == 2, "expected two chained pointers");
    check(patches[0].file_offset == 4096 && patches[0].value == 0x11234 &&
          !patches[0].binding, "64-offset rebase decoded incorrectly");
    check(patches[1].file_offset == 4104 && patches[1].value == 0x40003 &&
          patches[1].binding, "64-offset bind decoded incorrectly");
    auto modified = b;
    anyios::dyld::apply_chained_patches(modified, patches);
    check(get64(modified, 4096) == 0x11234 && get64(modified, 4104) == 0x40003,
          "fixup patch values incorrect");
    check(get64(b, 4096) != get64(modified, 4096), "planner mutated original bytes");
    b = image();
    put16(b, 256 + 46, 2);
    auto direct = anyios::dyld::plan_chained_fixups(b, anyios::macho::inspect(b), targets);
    check(direct[0].value == 0x1234, "absolute 64-bit rebase not decoded");

    b = image(); put16(b, 256 + 46, 9);
    rejects(b, targets, "unsupported chained pointer format");
    b = image(); put16(b, 256 + 62, 0x8000);
    rejects(b, targets, "multi-start");
    b = image(); put16(b, 256 + 62, 4093);
    rejects(b, targets, "chained page start invalid");
    b = image(); put64(b, 256 + 48, 0);
    rejects(b, targets, "segment offset disagrees");
    b = image(); put64(b, 4096, (std::uint64_t{1} << 44) | 0x1234);
    rejects(b, targets, "reserved chained rebase");
    b = image(); put64(b, 4104, (std::uint64_t{1} << 63) | 1);
    rejects(b, targets, "unresolved chained bind ordinal");
    b = image(); put64(b, 4096, (std::uint64_t{1024} << 51) | 0x1234);
    rejects(b, targets, "chained pointer crosses page");
    // Import format 2: signed 32-bit addend stored after the descriptor.
    b = image();
    put32(b, 188, 96);
    put32(b, 256 + 20, 2);
    put32(b, 256 + 12, 72);
    put32(b, 256 + 68, 0xfffffffbU);
    b[256 + 72] = std::byte{'f'};
    b[256 + 73] = std::byte{'o'};
    b[256 + 74] = std::byte{'o'};
    auto signed32 = anyios::dyld::plan_chained_fixups(
        b, anyios::macho::inspect(b), targets);
    check(signed32[1].value == 0x3fffe, "signed 32-bit import addend incorrect");

    // Import format 3: 64-bit descriptor and signed 64-bit addend.
    b = image();
    put32(b, 188, 96);
    put32(b, 256 + 20, 3);
    put32(b, 256 + 12, 80);
    put64(b, 256 + 64, 1);
    put64(b, 256 + 72, UINT64_MAX - 4);
    b[256 + 80] = std::byte{'f'};
    b[256 + 81] = std::byte{'o'};
    b[256 + 82] = std::byte{'o'};
    auto signed64 = anyios::dyld::plan_chained_fixups(
        b, anyios::macho::inspect(b), targets);
    check(signed64[1].value == 0x3fffe, "signed 64-bit import addend incorrect");
    put64(b, 256 + 72, INT64_MAX);
    rejects(b, {UINT64_MAX - 0x10}, "chained signed bind addend overflow");
    put64(b, 256 + 72, 0x8000000000000000ULL);
    rejects(b, targets, "chained signed bind addend underflow");

    b = image(); put32(b, 256 + 20, 4);
    rejects(b, targets, "unsupported chained imports format");
    b = image();
    rejects(b, {0}, "unresolved chained bind ordinal");

    b = image();
    auto valid_patches = anyios::dyld::plan_chained_fixups(b, anyios::macho::inspect(b), targets);
    valid_patches.push_back({8191, 0, false});
    const auto old = b;
    try {
        anyios::dyld::apply_chained_patches(b, valid_patches);
        throw std::runtime_error("out-of-range patch accepted");
    } catch (const anyios::macho::FormatError&) {
        check(b == old, "failed patch partially modified file");
    }
    valid_patches.back() = {4100, 1, false};
    try {
        anyios::dyld::apply_chained_patches(b, valid_patches);
        throw std::runtime_error("overlapping patch accepted");
    } catch (const anyios::macho::FormatError&) {
        check(b == old, "overlapping patch partially modified file");
    }
}
}
int main() {
    try {
        run();
        std::cout << "Chained 64-bit bind/rebase plan and atomic patch tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "chained fixup test failed: " << error.what() << '\n';
        return 1;
    }
}
