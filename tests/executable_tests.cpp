#include <anyios/executable.hpp>
#include <anyios/macho.hpp>

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

void w32(Bytes& data, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data.at(at + i) = std::byte((value >> (8 * i)) & 255);
}
void w64(Bytes& data, std::size_t at, std::uint64_t value) {
    w32(data, at, static_cast<std::uint32_t>(value));
    w32(data, at + 4, static_cast<std::uint32_t>(value >> 32));
}
void check(bool condition, std::string_view reason) {
    if (!condition) throw std::runtime_error(std::string(reason));
}
Bytes valid() {
    Bytes bytes(4096);
    w32(bytes, 0, 0xfeedfacf);
    w32(bytes, 4, 0x0100000c);
    w32(bytes, 12, 2);
    w32(bytes, 16, 3);
    w32(bytes, 20, 120);
    w32(bytes, 32, 0x19);
    w32(bytes, 36, 72);
    constexpr char text_name[] = "__TEXT";
    for (unsigned i = 0; i < sizeof(text_name) - 1; ++i) bytes.at(40 + i) = std::byte(text_name[i]);
    w64(bytes, 32 + 24, 0x10000);
    w64(bytes, 32 + 32, 4096);
    w64(bytes, 32 + 40, 0);
    w64(bytes, 32 + 48, 4096);
    w32(bytes, 32 + 56, 5);
    w32(bytes, 32 + 60, 5);
    w32(bytes, 104, 0x80000028);
    w32(bytes, 108, 24);
    w64(bytes, 112, 0x100);
    w32(bytes, 128, 0x32);
    w32(bytes, 132, 24);
    w32(bytes, 136, 2);
    w32(bytes, 140, 0x000d0000);
    w32(bytes, 144, 0x000d0000);
    w32(bytes, 148, 0);
    bytes[0x100] = std::byte{0x40};
    bytes[0x101] = std::byte{0x05};
    bytes[0x102] = std::byte{0x80};
    bytes[0x103] = std::byte{0xd2};
    bytes[0x104] = std::byte{0xc0};
    bytes[0x105] = std::byte{0x03};
    bytes[0x106] = std::byte{0x5f};
    bytes[0x107] = std::byte{0xd6};
    return bytes;
}
void rejects(const Bytes& input, std::string_view reason) {
    GuestMemory memory(0x10000, 0x2000);
    try {
        (void)anyios::loader::load_static_executable(input, memory);
    } catch (const anyios::macho::FormatError& e) {
        check(std::string_view(e.what()).find(reason) != std::string_view::npos, e.what());
        check(!memory.fetch(0x10100).has_value(), "failed image mutated guest memory");
        return;
    }
    throw std::runtime_error("unsupported executable was accepted");
}
void test() {
    auto b = valid();
    GuestMemory memory(0x10000, 0x2000);
    auto loaded = anyios::loader::load_static_executable(b, memory);
    check(loaded.entry_address == 0x10100 && loaded.mapped_segments == 1, "mapped executable entry");
    check(memory.fetch(0x10100) == 0xd2800540, "entry instruction");
    check(memory.fetch(0x10104) == 0xd65f03c0, "return instruction");
    check(!memory.write(0x10100, 0, 4), "W xor X policy");
    check(!memory.fetch(0x11000), "unmapped guard page");

    b = valid(); w64(b, 112, 4096);
    rejects(b, "LC_MAIN entry outside");
    b = valid(); w32(b, 32 + 60, 7);
    rejects(b, "permissions");
    b = valid(); w32(b, 32 + 60, 1);
    rejects(b, "__TEXT segment is missing");
    b = valid(); w64(b, 32 + 32, 12288);
    rejects(b, "mapping failed");
    b = valid(); w32(b, 136, 1);
    rejects(b, "only iOS");
    b = valid(); w32(b, 12, 1);
    rejects(b, "expected MH_EXECUTE");
    b = valid(); w32(b, 16, 4); w32(b, 20, 128);
    w32(b, 152, 0x2a); w32(b, 156, 8);
    rejects(b, "unsupported executable load command");
    b = valid(); w32(b, 148, 1);
    rejects(b, "tools exceed command size");
    b = valid(); w32(b, 16, 2);
    rejects(b, "load command count does not consume declared region");
    b = valid(); w32(b, 32 + 56, 1);
    rejects(b, "unsupported segment permissions");
    b = valid(); b.resize(8192); w64(b, 32 + 40, 0x100);
    rejects(b, "__TEXT does not contain Mach-O header");
    b = valid(); w64(b, 32 + 48, 128);
    rejects(b, "__TEXT does not contain Mach-O header");

    b = valid();
    b.resize(8192);
    w32(b, 16, 4);
    w32(b, 20, 192);
    w32(b, 152, 0x19);
    w32(b, 156, 72);
    constexpr char data_name[] = "__DATA";
    for (std::size_t i = 0; i < sizeof(data_name) - 1; ++i) {
        b[160 + i] = std::byte(data_name[i]);
    }
    w64(b, 152 + 24, 0x11000);
    w64(b, 152 + 32, 4096);
    w64(b, 152 + 40, 4096);
    w64(b, 152 + 48, 16);
    w32(b, 152 + 56, 3);
    w32(b, 152 + 60, 3);
    w64(b, 4096, 0x1122334455667788ULL);
    {
        GuestMemory segments(0x10000, 0x3000);
        const auto loaded_image = anyios::loader::load_static_executable(b, segments);
        check(loaded_image.entry_address == 0x10100 && loaded_image.mapped_segments == 2, "two mapped segments");
        check(segments.read(0x11000, 8) == 0x1122334455667788ULL, "data segment initialized");
        check(segments.read(0x11020, 8) == 0, "BSS is zero-filled");
        check(!segments.fetch(0x11000), "writable segment is not executable");
        check(!segments.write(0x10100, 0, 4), "text segment is not writable");
    }
    {
        auto malformed = b;
        w64(malformed, 152 + 40, 2048);
        rejects(malformed, "executable file segments overlap");
    }
    {
        auto malformed = b;
        w64(malformed, 152 + 24, 0x10000);
        rejects(malformed, "guest executable segment mapping failed");
    }
    {
        auto malformed = b;
        w32(malformed, 152 + 56, 1);
        rejects(malformed, "unsupported segment permissions");
    }
    {
        auto malformed = b;
        w32(malformed, 152 + 60, 5);
        rejects(malformed, "unsupported segment permissions");
    }

}
}
int main() {
    try {
        test();
        std::cout << "Restricted MH_EXECUTE guest mapping checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
