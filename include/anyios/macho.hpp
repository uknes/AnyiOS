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
};

struct Version {
    std::uint32_t platform;
    std::uint32_t minimum_os;
    std::uint32_t sdk;
};

struct Image {
    std::uint32_t cpu_subtype = 0;
    std::uint32_t file_type = 0;
    std::uint32_t command_count = 0;
    std::vector<Segment> segments;
    std::vector<std::string> libraries;
    std::vector<std::string> rpaths;
    std::vector<Version> versions;
    std::uint64_t entry_offset = 0;
    bool has_entry = false;
    bool is_encrypted = false;
};

Image inspect(std::span<const std::byte> bytes);
std::string version_string(std::uint32_t encoded);

}
