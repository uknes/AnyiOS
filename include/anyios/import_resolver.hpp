#pragma once

#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace anyios::dyld {

struct LoadedDylib {
    const macho::Image* image;
    std::uint64_t slide;
};

std::vector<std::uint64_t> resolve_chained_import_targets(
    std::span<const std::byte> importing_file,
    const macho::Image& importer,
    std::span<const LoadedDylib> loaded_libraries);

}
