#pragma once
#include <anyios/macho.hpp>
#include <cstdint>
#include <span>
#include <optional>
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
    std::uint64_t lazy_record_offset = 0;
};
struct LegacyCursorWrap {
    std::uint64_t opcode_offset;
    std::uint64_t before;
    std::uint64_t delta;
    std::uint64_t pointer_advance;
    std::uint64_t after;
};
struct LegacyBindStream {
    std::vector<LegacyBindSite> sites;
    std::vector<std::string> non_weak_definitions;
    std::uint64_t cursor_wraps = 0;
    std::optional<LegacyCursorWrap> first_cursor_wrap;
};
// Read-only pointer bind metadata. Weak coalescing and lazy dispatch are
// not implemented by these decoders. Threaded binds fail closed.
LegacyBindStream inspect_legacy_eager_bind_stream(
    std::span<const std::byte> file, const macho::Image& image);
std::vector<LegacyBindSite> inspect_legacy_eager_bind_sites(
    std::span<const std::byte> file, const macho::Image& image);
LegacyBindStream inspect_legacy_weak_bind_sites(
    std::span<const std::byte> file, const macho::Image& image);
std::vector<LegacyBindSite> inspect_legacy_lazy_bind_sites(
    std::span<const std::byte> file, const macho::Image& image);
}
