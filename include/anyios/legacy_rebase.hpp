#pragma once
#include <anyios/macho.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace anyios::dyld {
struct LegacyRebaseSite {
    std::uint64_t file_offset;
    std::uint64_t vm_address;
};
// This READ-ONLY planner NEVER patches input Mach-O or guest memory.
// For now only the Apple 64-bit pointer rebase kind is admitted.
std::vector<LegacyRebaseSite> inspect_legacy_rebase_sites(
    std::span<const std::byte> source, const macho::Image& image);
} // namespace anyios::dyld
