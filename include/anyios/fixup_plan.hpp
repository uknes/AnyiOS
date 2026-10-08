#pragma once

#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace anyios::dyld {

struct FixupPatch {
    std::uint64_t file_offset;
    std::uint64_t value;
    bool binding;
};

std::vector<FixupPatch> plan_chained_fixups(
    std::span<const std::byte> file,
    const macho::Image& image,
    std::span<const std::uint64_t> import_addresses);

void apply_chained_patches(
    std::span<std::byte> destination,
    std::span<const FixupPatch> patches);

}
