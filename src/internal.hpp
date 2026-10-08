#pragma once
#include <anyios/macho.hpp>
namespace anyios::macho {
Image inspect_thin(std::span<const std::byte> bytes);
std::vector<std::string> parse_chained_imports(std::span<const std::byte> bytes,
                                               std::uint32_t offset, std::uint32_t size);
std::vector<std::string> parse_exports(std::span<const std::byte> bytes,
                                       std::uint32_t offset, std::uint32_t size);
}
