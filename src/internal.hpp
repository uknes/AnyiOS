#pragma once
#include <anyios/macho.hpp>
namespace anyios::macho {
Image inspect_thin(std::span<const std::byte> bytes);
std::vector<Symbol> parse_symbols(std::span<const std::byte> bytes,
                                  std::uint32_t symbols_offset, std::uint32_t symbol_count,
                                  std::uint32_t strings_offset, std::uint32_t strings_size);
std::vector<std::string> parse_chained_imports(std::span<const std::byte> bytes,
                                               std::uint32_t offset, std::uint32_t size);
std::vector<Export> parse_exports(std::span<const std::byte> bytes,
                                       std::uint32_t offset, std::uint32_t size);
}
