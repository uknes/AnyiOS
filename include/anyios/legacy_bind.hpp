#pragma once
#include <anyios/macho.hpp>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace anyios::dyld {
struct LegacyBindSite {
    std::uint64_t file_offset;
    std::uint64_t vm_address;
    std::string symbol;
    std::int32_t library_ordinal;
    std::int64_t addend;
    bool weak_import;
};
// Bounded/read-only classic eager bind scanner. NOT lazy/weak/threaded bind,
// resolution, guest pointer rewriting, or a dyld runtime.
std::vector<LegacyBindSite> inspect_legacy_eager_bind_sites(
    std::span<const std::byte> file, const macho::Image& image);
}
