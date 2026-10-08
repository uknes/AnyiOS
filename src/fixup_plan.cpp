#include <anyios/fixup_plan.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace anyios::dyld {
namespace {
constexpr std::uint16_t pointer_64 = 2;
constexpr std::uint16_t pointer_64_offset = 6;
constexpr std::uint64_t bind_bit = std::uint64_t{1} << 63;

void require(std::span<const std::byte> bytes, std::uint64_t start,
             std::uint64_t length, const char* description) {
    if (start > bytes.size() || length > bytes.size() - start) {
        throw macho::FormatError(std::string(description) + " out of bounds");
    }
}

std::uint16_t u16(std::span<const std::byte> bytes, std::uint64_t at) {
    require(bytes, at, 2, "chained field");
    const auto index = static_cast<std::size_t>(at);
    return std::uint16_t(std::to_integer<std::uint8_t>(bytes[index])) |
           (std::uint16_t(std::to_integer<std::uint8_t>(bytes[index + 1])) << 8);
}
std::uint32_t u32(std::span<const std::byte> bytes, std::uint64_t at) {
    return std::uint32_t(u16(bytes, at)) | (std::uint32_t(u16(bytes, at + 2)) << 16);
}
std::uint64_t u64(std::span<const std::byte> bytes, std::uint64_t at) {
    return std::uint64_t(u32(bytes, at)) | (std::uint64_t(u32(bytes, at + 4)) << 32);
}
std::uint64_t plus(std::uint64_t a, std::uint64_t b, const char* subject) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        throw macho::FormatError(std::string(subject) + " overflow");
    }
    return a + b;
}
}

std::vector<FixupPatch> plan_chained_fixups(
    std::span<const std::byte> file,
    const macho::Image& image,
    std::span<const std::uint64_t> import_addresses) {
    if (image.is_fat) throw macho::FormatError("fixup planner requires a thin image");
    if (image.is_encrypted) throw macho::FormatError("encrypted fixup image unsupported");
    if (!image.chained_fixups_range) {
        if (!import_addresses.empty()) throw macho::FormatError("imports without fixups");
        return {};
    }
    const auto& range = *image.chained_fixups_range;
    require(file, range.file_offset, range.file_size, "chained payload");
    const auto payload = file.subspan(range.file_offset, range.file_size);
    require(payload, 0, 28, "chained fixup header");
    if (u32(payload, 0) != 0) throw macho::FormatError("unsupported chained fixup version");
    const auto starts = u32(payload, 4);
    const auto imports_at = u32(payload, 8);
    const auto imports_count = u32(payload, 16);
    const auto imports_format = u32(payload, 20);
    if (imports_format != 1) throw macho::FormatError("unsupported chained import addend format");
    if (imports_count != import_addresses.size() || imports_count != image.chained_imports.size()) {
        throw macho::FormatError("chained import target count mismatch");
    }
    require(payload, imports_at, std::uint64_t(imports_count) * 4, "chained imports");
    require(payload, starts, 4, "chained segment starts");
    const auto segments = u32(payload, starts);
    if (segments != image.segments.size() || segments > 4096) {
        throw macho::FormatError("chained segment count does not match image");
    }
    require(payload, std::uint64_t(starts) + 4, std::uint64_t(segments) * 4,
            "chained segment starts table");
    const auto base_it = std::find_if(image.segments.begin(), image.segments.end(),
        [](const macho::Segment& segment) { return segment.name == "__TEXT"; });
    if (base_it == image.segments.end()) {
        throw macho::FormatError("chained image has no __TEXT base");
    }
    const std::uint64_t image_base = base_it->vm_address;
    std::vector<FixupPatch> patches;
    std::unordered_set<std::uint64_t> visited_addresses;
    for (std::uint32_t seg_index = 0; seg_index < segments; ++seg_index) {
        const auto relative = u32(payload, std::uint64_t(starts) + 4 + std::uint64_t(seg_index) * 4);
        if (relative == 0) continue;
        const auto at = plus(starts, relative, "chained starts");
        require(payload, at, 22, "chained segment header");
        const auto header_size = u32(payload, at);
        if (header_size < 22) throw macho::FormatError("chained segment header too small");
        require(payload, at, header_size, "chained segment starts");
        const auto page_size = u16(payload, at + 4);
        const auto format = u16(payload, at + 6);
        const auto offset = u64(payload, at + 8);
        const auto page_count = u16(payload, at + 20);
        if (header_size < 22 + std::uint32_t(page_count) * 2) {
            throw macho::FormatError("chained page table truncated");
        }
        if (page_size != 4096 && page_size != 16384) {
            throw macho::FormatError("unsupported chained page size");
        }
        if (format != pointer_64 && format != pointer_64_offset) {
            throw macho::FormatError("unsupported chained pointer format");
        }
        const auto& segment = image.segments[seg_index];
        if (segment.vm_address < image_base || offset != segment.vm_address - image_base) {
            throw macho::FormatError("chained segment offset disagrees with Mach-O");
        }
        for (std::uint32_t page = 0; page < page_count; ++page) {
            const auto first = u16(payload, at + 22 + std::uint64_t(page) * 2);
            if (first == 0xffff) continue;
            if ((first & 0x8000) != 0) {
                throw macho::FormatError("multi-start chained page unsupported");
            }
            if (first > page_size - 8) throw macho::FormatError("chained page start invalid");
            auto within = plus(std::uint64_t(page) * page_size, first, "chained page offset");
            for (;;) {
                if (within > segment.file_size || segment.file_size - within < 8 ||
                    within > segment.vm_size || segment.vm_size - within < 8 ||
                    (within % page_size) > page_size - 8) {
                    throw macho::FormatError("chained pointer outside segment");
                }
                const auto location = plus(segment.file_offset, within, "chained file location");
                require(file, location, 8, "chained pointer bytes");
                if (!visited_addresses.insert(location).second) {
                    throw macho::FormatError("duplicate chained pointer");
                }
                if (patches.size() >= 65536) {
                    throw macho::FormatError("chained fixup safety limit exceeded");
                }
                const auto raw = u64(file, location);
                const auto next = (raw >> 51) & 0xfff;
                std::uint64_t value = 0;
                const bool binding = (raw & bind_bit) != 0;
                if (binding) {
                    if ((raw & (std::uint64_t{0x7ffff} << 32)) != 0) {
                        throw macho::FormatError("reserved chained bind bits set");
                    }
                    const auto ordinal = raw & 0xffffff;
                    if (ordinal >= import_addresses.size() || import_addresses[ordinal] == 0) {
                        throw macho::FormatError("unresolved chained bind ordinal");
                    }
                    const auto addend = (raw >> 24) & 0xff;
                    value = plus(import_addresses[ordinal], addend, "chained bind address");
                } else {
                    if ((raw & (std::uint64_t{0x7f} << 44)) != 0) {
                        throw macho::FormatError("reserved chained rebase bits set");
                    }
                    const auto target = raw & ((std::uint64_t{1} << 36) - 1);
                    const auto top = ((raw >> 36) & 0xff) << 56;
                    value = plus(format == pointer_64_offset ? image_base : 0,
                                 target | top, "chained rebase address");
                }
                patches.push_back({location, value, binding});
                if (next == 0) break;
                const auto advance = next * 4;
                const auto next_within = plus(within, advance, "chained delta");
                if (next_within / page_size != within / page_size) {
                    throw macho::FormatError("chained pointer crosses page");
                }
                within = next_within;
            }
        }
    }
    return patches;
}

void apply_chained_patches(
    std::span<std::byte> destination,
    std::span<const FixupPatch> patches) {
    std::vector<std::uint64_t> offsets;
    offsets.reserve(patches.size());
    for (const auto& patch : patches) {
        if (patch.file_offset > destination.size() ||
            destination.size() - patch.file_offset < 8) {
            throw macho::FormatError("fixup patch outside file");
        }
        offsets.push_back(patch.file_offset);
    }
    std::sort(offsets.begin(), offsets.end());
    for (std::size_t i = 1; i < offsets.size(); ++i) {
        if (offsets[i] - offsets[i - 1] < 8) {
            throw macho::FormatError("overlapping fixup patches");
        }
    }
    for (const auto& patch : patches) {
        for (unsigned i = 0; i < 8; ++i) {
            destination[static_cast<std::size_t>(patch.file_offset) + i] =
                std::byte((patch.value >> (8 * i)) & 0xff);
        }
    }
}
}
