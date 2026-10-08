#include <anyios/object_code.hpp>
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

void put32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        bytes.at(at + i) = std::byte((value >> (8 * i)) & 255);
    }
}
void put64(Bytes& bytes, std::size_t at, std::uint64_t value) {
    put32(bytes, at, static_cast<std::uint32_t>(value));
    put32(bytes, at + 4, static_cast<std::uint32_t>(value >> 32));
}
void name(Bytes& bytes, std::size_t at, std::string_view text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        bytes.at(at + i) = std::byte(text[i]);
    }
}
Bytes owned_object() {
    Bytes bytes(248);
    put32(bytes, 0, 0xfeedfacf);
    put32(bytes, 4, 0x0100000c);
    put32(bytes, 12, 1);
    put32(bytes, 16, 2);
    put32(bytes, 20, 176);

    put32(bytes, 32, 0x19);
    put32(bytes, 36, 152);
    name(bytes, 40, "__TEXT");
    put32(bytes, 32 + 64, 1);
    name(bytes, 104, "__text");
    name(bytes, 120, "__TEXT");
    put64(bytes, 104 + 40, 8);
    put32(bytes, 104 + 48, 208);

    put32(bytes, 184, 0x2);
    put32(bytes, 188, 24);
    put32(bytes, 192, 216);
    put32(bytes, 196, 1);
    put32(bytes, 200, 232);
    put32(bytes, 204, 16);

    const std::array<std::byte, 8> code{
        std::byte{0x40}, std::byte{0x05}, std::byte{0x80}, std::byte{0xd2},
        std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
    };
    for (std::size_t i = 0; i < code.size(); ++i) bytes[208 + i] = code[i];
    put32(bytes, 216, 1);
    bytes[220] = std::byte{0x0f};
    bytes[221] = std::byte{1};
    name(bytes, 233, "_anyios_answer");
    return bytes;
}

void require(bool test, std::string_view message) {
    if (!test) throw std::runtime_error(std::string(message));
}
void rejection(const Bytes& bytes, std::string_view symbol, std::string_view error) {
    try {
        static_cast<void>(anyios::loader::extract_object_function(bytes, symbol));
    } catch (const anyios::macho::FormatError& ex) {
        require(std::string_view(ex.what()).find(error) != std::string_view::npos,
                std::string("unexpected error: ") + ex.what());
        return;
    }
    throw std::runtime_error("invalid object code accepted");
}
void tests() {
    auto bytes = owned_object();
    const auto result = anyios::loader::extract_object_function(bytes, "_anyios_answer");
    require(result.code.size() == 8, "code length");
    require(result.code[0] == std::byte{0x40} && result.code[7] == std::byte{0xd6},
            "code bytes");

    rejection(bytes, "_missing", "not found");

    auto broken = bytes;
    put32(broken, 12, 2);
    rejection(broken, "_anyios_answer", "MH_OBJECT");

    broken = bytes;
    broken[220] = std::byte{0x01};
    rejection(broken, "_anyios_answer", "not defined in a section");

    broken = owned_object();
    broken[221] = std::byte{2};
    rejection(broken, "_anyios_answer", "not defined in a section");

    broken = owned_object();
    put64(broken, 224, 8);
    rejection(broken, "_anyios_answer", "outside its section");

    broken = owned_object();
    put32(broken, 104 + 60, 1);
    put32(broken, 104 + 56, 208);
    rejection(broken, "_anyios_answer", "relocated object code");

    broken = owned_object();
    name(broken, 104, "__data");
    rejection(broken, "_anyios_answer", "text section");

    broken = owned_object();
    put32(broken, 104 + 40, 6);
    rejection(broken, "_anyios_answer", "ARM64 length");
}
}

int main() {
    try {
        tests();
        std::cout << "Owned Mach-O code extraction tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
