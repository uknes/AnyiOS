#include <anyios/macho.hpp>
#include <algorithm>
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
void put32(Bytes& b, std::size_t pos, std::uint32_t value, bool big = false) {
    for (unsigned i = 0; i < 4; ++i) {
        const auto shift = big ? (3 - i) * 8 : i * 8;
        b.at(pos + i) = std::byte((value >> shift) & 0xff);
    }
}
void put64(Bytes& b, std::size_t pos, std::uint64_t value, bool big = false) {
    put32(b, pos, static_cast<std::uint32_t>(big ? value >> 32 : value), big);
    put32(b, pos + 4, static_cast<std::uint32_t>(big ? value : value >> 32), big);
}
Bytes thin() {
    Bytes b(32);
    put32(b, 0, 0xfeedfacf);
    put32(b, 4, 0x0100000c);
    put32(b, 12, 1);
    return b;
}
Bytes fat(bool wide, bool big) {
    const auto object = thin();
    const std::size_t slice_at = wide ? 40 : 32;
    Bytes b(slice_at + object.size());
    const auto magic = wide ? 0xcafebabf : 0xcafebabe;
    put32(b, 0, magic, big);
    put32(b, 4, 1, big);
    put32(b, 8, 0x0100000c, big);
    put32(b, 12, 0, big);
    if (wide) {
        put64(b, 16, slice_at, big);
        put64(b, 24, object.size(), big);
        put32(b, 32, 3, big);
    } else {
        put32(b, 16, static_cast<std::uint32_t>(slice_at), big);
        put32(b, 20, static_cast<std::uint32_t>(object.size()), big);
        put32(b, 24, 3, big);
    }
    std::copy(object.begin(), object.end(), b.begin() + static_cast<std::ptrdiff_t>(slice_at));
    return b;
}
Bytes linkedit(std::uint32_t kind, Bytes payload) {
    Bytes b(48 + payload.size());
    put32(b, 0, 0xfeedfacf);
    put32(b, 4, 0x0100000c);
    put32(b, 12, 2);
    put32(b, 16, 1);
    put32(b, 20, 16);
    put32(b, 32, kind);
    put32(b, 36, 16);
    put32(b, 40, 48);
    put32(b, 44, static_cast<std::uint32_t>(payload.size()));
    std::copy(payload.begin(), payload.end(), b.begin() + 48);
    return b;
}
Bytes import_data() {
    Bytes p(40);
    put32(p, 4, 28);
    put32(p, 8, 32);
    put32(p, 12, 36);
    put32(p, 16, 1);
    put32(p, 20, 1);
    p[36] = std::byte{'f'};
    p[37] = std::byte{'o'};
    p[38] = std::byte{'o'};
    return p;
}
void require(bool yes, std::string_view message) {
    if (!yes) throw std::runtime_error(std::string(message));
}
void rejects(const Bytes& bytes, std::string_view expected) {
    try {
        static_cast<void>(anyios::macho::inspect(bytes));
    } catch (const anyios::macho::FormatError& error) {
        require(std::string_view(error.what()).find(expected) != std::string_view::npos,
                std::string("wrong error: ") + error.what());
        return;
    }
    throw std::runtime_error("malformed binary accepted");
}
void test_universal() {
    for (const bool wide : {false, true}) for (const bool big : {false, true}) {
        auto input = fat(wide, big);
        const auto result = anyios::macho::inspect(input);
        require(result.is_fat && result.architecture_count == 1, "container metadata");
        require(result.file_type == 1 && result.slice_size == 32, "selected Mach-O");
        auto broken = input;
        put32(broken, 4, 0xffffffff, big);
        rejects(broken, "count exceeds safety limit");
        broken = input;
        put32(broken, 8, 0x01000007, big);
        rejects(broken, "no ARM64");
        broken = input;
        put32(broken, 12, 2, big);
        rejects(broken, "subtype");
        broken = input;
        put32(broken, wide ? 32 : 24, 64, big);
        rejects(broken, "alignment invalid");
        broken = input;
        put32(broken, wide ? 32 : 24, 16, big);
        rejects(broken, "misaligned");
        broken = input;
        if (wide) put64(broken, 16, UINT64_MAX, big);
        else put32(broken, 16, UINT32_MAX, big);
        rejects(broken, "slice out of bounds");
    }
}
void test_linkedit() {
    auto p = import_data();
    auto result = anyios::macho::inspect(linkedit(0x80000034, p));
    require(result.has_chained_fixups && result.chained_imports.size() == 1 &&
            result.chained_imports[0] == "foo", "chained import");
    put32(p, 16, UINT32_MAX);
    rejects(linkedit(0x80000034, p), "count exceeds safety limit");
    p = import_data();
    put32(p, 24, 1);
    rejects(linkedit(0x80000034, p), "compression");
    p = import_data();
    put32(p, 4, 39);
    rejects(linkedit(0x80000034, p), "segment starts");
    p = import_data();
    p[39] = std::byte{'X'};
    rejects(linkedit(0x80000034, p), "NUL-terminated");
    p = import_data();
    put32(p, 20, 8);
    rejects(linkedit(0x80000034, p), "imports format");
    Bytes trie{std::byte{0}, std::byte{1}, std::byte{'a'}, std::byte{0},
               std::byte{5}, std::byte{2}, std::byte{0}, std::byte{42}, std::byte{0}};
    result = anyios::macho::inspect(linkedit(0x80000033, trie));
    require(result.has_export_trie && result.exported_symbols.size() == 1 &&
            result.exported_symbols[0] == "a", "export trie");
    trie[4] = std::byte{0};
    rejects(linkedit(0x80000033, trie), "cycle");
    trie[4] = std::byte{22};
    rejects(linkedit(0x80000033, trie), "child offset");
    rejects(linkedit(0x80000033, Bytes{std::byte{0x80}}), "ULEB128");
}
void test_mutations() {
    std::uint32_t seed = 0x58a6bc32;
    for (const auto& original : {fat(true, true), linkedit(0x80000034, import_data())}) {
        for (unsigned i = 0; i < 3000; ++i) {
            auto mutated = original;
            seed = seed * 1664525 + 1013904223;
            const auto index = static_cast<std::size_t>(seed) % mutated.size();
            mutated[index] = std::byte((seed >> 16) & 0xff);
            try {
                static_cast<void>(anyios::macho::inspect(mutated));
            } catch (const anyios::macho::FormatError&) {
            }
        }
    }
}
}
int main() {
    try {
        test_universal();
        test_linkedit();
        test_mutations();
        std::cout << "Universal Mach-O and dyld metadata regression checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "M1 regression failure: " << error.what() << "\n";
        return 1;
    }
}
