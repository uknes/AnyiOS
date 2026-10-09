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
void w32(Bytes& b, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) b.at(at + i) = std::byte((value >> (i * 8)) & 255);
}
void w64(Bytes& b, std::size_t at, std::uint64_t value) {
    w32(b, at, static_cast<std::uint32_t>(value));
    w32(b, at + 4, static_cast<std::uint32_t>(value >> 32));
}
Bytes fixture() {
    Bytes b(81);
    w32(b, 0, 0xfeedfacf);
    w32(b, 4, 0x0100000c);
    w32(b, 12, 1);
    w32(b, 16, 1);
    w32(b, 20, 24);
    w32(b, 32, 0x2);
    w32(b, 36, 24);
    w32(b, 40, 56);
    w32(b, 44, 1);
    w32(b, 48, 72);
    w32(b, 52, 9);
    w32(b, 56, 1);
    b[60] = std::byte{0x0f};
    b[61] = std::byte{1};
    w64(b, 64, 0x1234);
    const std::string str = std::string("\0_symbol\0", 9);
    for (std::size_t i = 0; i < str.size(); ++i) b[72 + i] = std::byte(str[i]);
    return b;
}
void check(bool yes, std::string_view message) {
    if (!yes) throw std::runtime_error(std::string(message));
}
void rejects(const Bytes& data, std::string_view expected) {
    try { static_cast<void>(anyios::macho::inspect(data)); }
    catch (const anyios::macho::FormatError& error) {
        check(std::string_view(error.what()).find(expected) != std::string_view::npos,
              std::string("unexpected error: ") + error.what());
        return;
    }
    throw std::runtime_error("malformed data accepted");
}
void test_symtab() {
    auto b = fixture();
    auto parsed = anyios::macho::inspect(b);
    check(parsed.symbols.size() == 1 && parsed.symbols[0].name == "_symbol", "symbol name");
    check(parsed.symbols[0].value == 0x1234 && parsed.symbols[0].type == 0x0f, "symbol metadata");
    w32(b, 44, anyios::macho::max_symbol_table_entries + 1);
    rejects(b, "symbol count exceeds safety limit");
    b = fixture();
    w32(b, 40, 0xfffffff0);
    rejects(b, "symbol table out of bounds");
    b = fixture();
    w32(b, 48, 0xfffffff0);
    rejects(b, "symbol string table out of bounds");
    b = fixture();
    w32(b, 56, 9);
    rejects(b, "symbol name offset out of bounds");
    b = fixture();
    b.back() = std::byte{'x'};
    rejects(b, "not NUL-terminated");
    b = fixture();
    w32(b, 16, 2);
    w32(b, 20, 48);
    w32(b, 56, 2);
    w32(b, 60, 24);
    rejects(b, "duplicate LC_SYMTAB");
}
Bytes large_table(std::uint32_t count, std::size_t name_length) {
    constexpr std::size_t symbols_at = 56;
    const auto strings_at = symbols_at + std::size_t(count) * 16;
    Bytes b(strings_at + name_length + 2);
    w32(b, 0, 0xfeedfacf); w32(b, 4, 0x0100000c); w32(b, 12, 6);
    w32(b, 16, 1); w32(b, 20, 24); w32(b, 32, 2); w32(b, 36, 24);
    w32(b, 40, symbols_at); w32(b, 44, count);
    w32(b, 48, static_cast<std::uint32_t>(strings_at));
    w32(b, 52, static_cast<std::uint32_t>(name_length + 2));
    std::fill(b.begin() + static_cast<std::ptrdiff_t>(strings_at + 1), b.end() - 1, std::byte{'s'});
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = symbols_at + std::size_t(i) * 16;
        w32(b, at, 1);
        b[at + 4] = std::byte{0x24}; // N_FUN debug entry: retained, not an export.
        w64(b, at + 8, i);
    }
    return b;
}
void test_large_tables() {
    auto b = large_table(100001, 1);
    auto parsed = anyios::macho::inspect(b);
    check(parsed.symbols.size() == 100001 && parsed.symbols.back().value == 100000,
          "large debug table lost entries or order");
    check(!anyios::macho::is_defined_external_symbol(parsed.symbols.back()),
          "debug entry became a public export");
    w32(b, 56 + std::size_t(100000) * 16, 3);
    rejects(b, "symbol name offset out of bounds");
    b = large_table(40000, 256); // More than the former 8 MiB copied-name cap.
    parsed = anyios::macho::inspect(b);
    check(parsed.symbols.size() == 40000 && parsed.symbols.back().name.size() == 256,
          "large valid aggregate names rejected");
    b = large_table(4097, 16383); // Repeated string still consumes the copy budget.
    rejects(b, "symbol names exceed safety limit");
    b = large_table(1, 16384);
    rejects(b, "symbol name exceeds length limit");
    b = fixture();
    w32(b, 44, anyios::macho::max_symbol_table_entries);
    rejects(b, "symbol table out of bounds");
}
void test_indirect() {
    Bytes b(112);
    w32(b, 0, 0xfeedfacf); w32(b, 4, 0x0100000c);
    w32(b, 16, 1); w32(b, 20, 80);
    w32(b, 32, 0xb); w32(b, 36, 80);
    rejects(b, "requires LC_SYMTAB");
    w32(b, 36, 72);
    rejects(b, "truncated LC_DYSYMTAB");
}
void test_sections() {
    Bytes b(200);
    w32(b, 0, 0xfeedfacf); w32(b, 4, 0x0100000c);
    w32(b, 16, 1); w32(b, 20, 152);
    w32(b, 32, 0x19); w32(b, 36, 152);
    w64(b, 32 + 32, 4);
    w32(b, 32 + 64, 1);
    w32(b, 32 + 72 + 48, 184);
    w64(b, 32 + 72 + 40, 8);
    auto result = anyios::macho::inspect(b);
    check(result.sections.size() == 1 && result.sections[0].size == 8, "section metadata");
    w32(b, 32 + 72 + 48, 199);
    rejects(b, "section contents");
    w32(b, 32 + 72 + 64, 1);
    result = anyios::macho::inspect(b);
    check(result.sections[0].zero_fill, "zero-fill section");
    w32(b, 32 + 72 + 60, 1);
    w32(b, 32 + 72 + 56, 199);
    rejects(b, "section relocations");
}
}
int main() {
    try {
        test_symtab();
        test_large_tables();
        test_indirect();
        test_sections();
        std::cout << "Symbol and section checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Symbol and section checks failed: " << error.what() << "\n";
        return 1;
    }
}
