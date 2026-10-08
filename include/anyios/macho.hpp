#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace anyios::macho {

struct FormatError final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Segment {
    std::string name;
    std::uint64_t vm_address;
    std::uint64_t vm_size;
    std::uint64_t file_offset;
    std::uint64_t file_size;
    std::uint32_t sections;
    std::uint32_t init_protection = 0;
};

struct Version {
    std::uint32_t platform;
    std::uint32_t minimum_os;
    std::uint32_t sdk;
};

struct LinkeditRange {
    std::uint32_t file_offset;
    std::uint32_t file_size;
};

struct Section {
    std::string name;
    std::string segment_name;
    std::uint64_t address;
    std::uint64_t size;
    std::uint32_t file_offset;
    std::uint32_t relocation_count;
    bool zero_fill;
};

struct Symbol {
    std::string name;
    std::uint64_t value;
    std::uint8_t type;
    std::uint8_t section_index;
};

struct Image {
    std::uint32_t cpu_subtype = 0;
    std::uint32_t file_type = 0;
    std::uint32_t command_count = 0;
    std::vector<Segment> segments;
    std::vector<Section> sections;
    std::vector<Symbol> symbols;
    std::uint32_t indirect_symbol_count = 0;
    std::vector<std::string> libraries;
    std::vector<std::string> rpaths;
    std::vector<std::string> chained_imports;
    std::vector<std::string> exported_symbols;
    bool has_chained_fixups = false;
    bool has_export_trie = false;
    std::vector<Version> versions;
    std::uint64_t entry_offset = 0;
    bool has_entry = false;
    bool is_encrypted = false;
    bool is_fat = false;
    std::uint32_t architecture_count = 1;
    std::uint64_t slice_offset = 0;
    std::uint64_t slice_size = 0;
};

Image inspect(std::span<const std::byte> bytes);
std::string version_string(std::uint32_t encoded);

}
