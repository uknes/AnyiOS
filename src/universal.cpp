#include <anyios/macho.hpp>
#include "internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace anyios::macho {
namespace {
constexpr std::uint32_t arm64 = 0x0100000c;

std::uint32_t read32(std::span<const std::byte> bytes, std::size_t at, bool big_endian) {
    if (at > bytes.size() || 4 > bytes.size() - at) {
        throw FormatError("universal header out of bounds");
    }
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto shift = big_endian ? 8 * (3 - i) : 8 * i;
        result |= std::uint32_t(std::to_integer<std::uint8_t>(bytes[at + i])) << shift;
    }
    return result;
}

std::uint64_t read64(std::span<const std::byte> bytes, std::size_t at, bool big_endian) {
    const auto a = read32(bytes, at, big_endian);
    const auto b = read32(bytes, at + 4, big_endian);
    return big_endian ? (std::uint64_t(a) << 32) | b : (std::uint64_t(b) << 32) | a;
}

struct Slice {
    std::uint32_t cpu;
    std::uint32_t subtype;
    std::uint64_t offset;
    std::uint64_t size;
};

bool overlaps(const Slice& a, const Slice& b) {
    return a.size && b.size && a.offset < b.offset + b.size && b.offset < a.offset + a.size;
}

}

Image inspect(std::span<const std::byte> bytes) {
    if (bytes.size() < 4) throw FormatError("Mach-O magic out of bounds");
    const auto bytes0 = std::to_integer<unsigned>(bytes[0]);
    const auto bytes1 = std::to_integer<unsigned>(bytes[1]);
    const auto bytes2 = std::to_integer<unsigned>(bytes[2]);
    const auto bytes3 = std::to_integer<unsigned>(bytes[3]);
    const bool fat64 = (bytes0 == 0xca && bytes1 == 0xfe && bytes2 == 0xba && bytes3 == 0xbf) ||
                       (bytes0 == 0xbf && bytes1 == 0xba && bytes2 == 0xfe && bytes3 == 0xca);
    const bool fat32 = (bytes0 == 0xca && bytes1 == 0xfe && bytes2 == 0xba && bytes3 == 0xbe) ||
                       (bytes0 == 0xbe && bytes1 == 0xba && bytes2 == 0xfe && bytes3 == 0xca);
    if (!fat32 && !fat64) return inspect_thin(bytes);

    const bool big_endian = bytes0 == 0xca;
    if (bytes.size() < 8) throw FormatError("truncated universal header");
    const auto count = read32(bytes, 4, big_endian);
    if (count > 4096) throw FormatError("universal architecture count exceeds safety limit");
    const std::uint64_t entry_size = fat64 ? 32 : 20;
    const auto table_size = std::uint64_t(count) * entry_size;
    if (table_size > bytes.size() - 8) throw FormatError("universal architecture table out of bounds");
    const auto header_size = std::uint64_t(8) + table_size;

    std::vector<Slice> slices;
    slices.reserve(count);
    std::optional<std::size_t> selected;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = static_cast<std::size_t>(8 + std::uint64_t(i) * entry_size);
        Slice slice{read32(bytes, at, big_endian), read32(bytes, at + 4, big_endian),
                    fat64 ? read64(bytes, at + 8, big_endian) : read32(bytes, at + 8, big_endian),
                    fat64 ? read64(bytes, at + 16, big_endian) : read32(bytes, at + 12, big_endian)};
        const auto align = read32(bytes, at + (fat64 ? 24 : 16), big_endian);
        if (align > 63) throw FormatError("universal slice alignment invalid");
        if (slice.offset < header_size || slice.offset > bytes.size() ||
            slice.size > bytes.size() - slice.offset) {
            throw FormatError("universal slice out of bounds");
        }
        if ((slice.offset & ((std::uint64_t(1) << align) - 1)) != 0) {
            throw FormatError("universal slice offset is misaligned");
        }
        for (const auto& prior : slices) {
            if (overlaps(slice, prior)) throw FormatError("overlapping universal slices");
        }
        if (slice.cpu == arm64 && (!selected || ((slice.subtype & 0xff) != 2 &&
            (slices[*selected].subtype & 0xff) == 2))) {
            selected = slices.size();
        }
        slices.push_back(slice);
    }
    if (!selected) throw FormatError("universal binary has no ARM64 slice");
    const auto& chosen = slices[*selected];
    const auto offset = static_cast<std::size_t>(chosen.offset);
    const auto size = static_cast<std::size_t>(chosen.size);
    auto result = inspect_thin(bytes.subspan(offset, size));
    if (result.cpu_subtype != chosen.subtype) {
        throw FormatError("universal slice CPU subtype does not match its Mach-O header");
    }
    result.is_fat = true;
    result.architecture_count = count;
    result.slice_offset = chosen.offset;
    result.slice_size = chosen.size;
    return result;
}
}
