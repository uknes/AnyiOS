#include "internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace anyios::macho {
bool is_defined_external_symbol(const Symbol& symbol) {
    if (symbol.name.empty() || (symbol.type & 0xf0) != 0 || (symbol.type & 1) == 0) return false;
    const auto kind = symbol.type & 0x0e;
    return (kind == 0x0e && symbol.section_index != 0) || kind == 0x02;
}
namespace {
void require(std::span<const std::byte> bytes, std::size_t offset, std::size_t length,
             std::string_view subject) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw FormatError(std::string(subject) + " out of bounds");
    }
}
std::uint32_t read32(std::span<const std::byte> bytes, std::size_t offset) {
    require(bytes, offset, 4, "symbol field");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= std::uint32_t(std::to_integer<std::uint8_t>(bytes[offset + i])) << (8 * i);
    }
    return value;
}
std::uint64_t read64(std::span<const std::byte> bytes, std::size_t offset) {
    return std::uint64_t(read32(bytes, offset)) | (std::uint64_t(read32(bytes, offset + 4)) << 32);
}
}

std::vector<Symbol> parse_symbols(std::span<const std::byte> bytes,
                                  std::uint32_t symbols_offset, std::uint32_t symbol_count,
                                  std::uint32_t strings_offset, std::uint32_t strings_size) {
    if (symbol_count > 100000) throw FormatError("symbol count exceeds safety limit");
    require(bytes, symbols_offset, std::size_t(symbol_count) * 16, "symbol table");
    require(bytes, strings_offset, strings_size, "symbol string table");
    const auto strings = bytes.subspan(strings_offset, strings_size);
    std::vector<Symbol> symbols;
    symbols.reserve(symbol_count);
    std::size_t total_names = 0;
    for (std::uint32_t i = 0; i < symbol_count; ++i) {
        const auto at = std::size_t(symbols_offset) + std::size_t(i) * 16;
        const auto strx = read32(bytes, at);
        std::string name;
        if (strx) {
            if (strx >= strings.size()) throw FormatError("symbol name offset out of bounds");
            std::size_t end = strx;
            while (end < strings.size() && strings[end] != std::byte{0} && end - strx < 16384) ++end;
            if (end - strx == 16384) throw FormatError("symbol name exceeds length limit");
            if (end == strings.size()) throw FormatError("symbol name not NUL-terminated");
            const auto length = end - strx;
            if (length > 8 * 1024 * 1024 - total_names) throw FormatError("symbol names exceed safety limit");
            total_names += length;
            name.assign(reinterpret_cast<const char*>(strings.data() + strx), length);
        }
        symbols.push_back({std::move(name), read64(bytes, at + 8),
                          std::to_integer<std::uint8_t>(bytes[at + 4]),
                          std::to_integer<std::uint8_t>(bytes[at + 5])});
    }
    return symbols;
}
}
