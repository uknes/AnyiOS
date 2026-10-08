#include <anyios/executable.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace anyios::loader {
namespace {
constexpr std::uint32_t lc_segment_64 = 0x19;
constexpr std::uint32_t lc_main = 0x80000028;
constexpr std::uint32_t lc_build_version = 0x32;
constexpr std::uint32_t lc_min_iphoneos = 0x25;

std::uint32_t word(std::span<const std::byte> data, std::size_t at) {
    if (at > data.size() || 4 > data.size() - at) {
        throw macho::FormatError("executable command out of bounds");
    }
    std::uint32_t result = 0;
    for (unsigned i = 0; i != 4; ++i) {
        result |= std::uint32_t(std::to_integer<std::uint8_t>(data[at + i])) << (8 * i);
    }
    return result;
}

void verify_commands(std::span<const std::byte> file, std::uint32_t count) {
    std::size_t cursor = 32;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto cmd = word(file, cursor);
        const auto size = word(file, cursor + 4);
        if (cmd != lc_segment_64 && cmd != lc_main && cmd != lc_build_version &&
            cmd != lc_min_iphoneos) {
            throw macho::FormatError("unsupported executable load command");
        }
        cursor += size;
    }
}
}

LoadedExecutable load_static_executable(std::span<const std::byte> file,
                                        cpu::GuestMemory& memory) {
    const auto image = macho::inspect(file);
    if (image.is_fat) throw macho::FormatError("static execution requires a thin ARM64 image");
    if (image.is_encrypted) throw macho::FormatError("encrypted executable unsupported");
    if (image.file_type != 2) throw macho::FormatError("expected MH_EXECUTE");
    if (!image.has_entry) throw macho::FormatError("LC_MAIN entry point is required");
    if (!image.libraries.empty() || !image.rpaths.empty() ||
        image.has_chained_fixups || image.has_export_trie) {
        throw macho::FormatError("executable needs unsupported dynamic linking");
    }
    if (image.versions.empty()) throw macho::FormatError("iOS platform declaration required");
    for (const auto& version : image.versions) {
        if (version.platform != 2) throw macho::FormatError("only iOS ARM64 executable images supported");
    }
    verify_commands(file, image.command_count);

    auto draft = memory;
    const macho::Segment* text = nullptr;
    std::size_t count = 0;
    for (const auto& segment : image.segments) {
        if (segment.name == "__PAGEZERO" && segment.file_size == 0 &&
            segment.init_protection == 0) {
            continue;
        }
        if (segment.vm_size == 0 || segment.vm_size % cpu::GuestMemory::page_size != 0 ||
            segment.vm_address % cpu::GuestMemory::page_size != 0) {
            throw macho::FormatError("executable segment is not page aligned");
        }
        const auto permissions = segment.init_protection;
        if (permissions == 0 || (permissions & ~7u) ||
            ((permissions & 2u) && (permissions & 4u))) {
            throw macho::FormatError("unsupported segment permissions");
        }
        if (!draft.map(segment.vm_address, static_cast<std::size_t>(segment.vm_size), permissions)) {
            throw macho::FormatError("guest executable segment mapping failed");
        }
        if (segment.file_size &&
            !draft.load(segment.vm_address,
                file.subspan(static_cast<std::size_t>(segment.file_offset),
                             static_cast<std::size_t>(segment.file_size)))) {
            throw macho::FormatError("guest executable segment initialization failed");
        }
        if (segment.name == "__TEXT") {
            if (text) throw macho::FormatError("duplicate __TEXT segment");
            text = &segment;
        }
        ++count;
    }
    if (!text || !(text->init_protection & 4u) || !(text->init_protection & 1u)) {
        throw macho::FormatError("executable __TEXT segment is missing");
    }
    if (image.entry_offset > text->file_size ||
        text->file_size - image.entry_offset < 4 ||
        (image.entry_offset & 3u) != 0 ||
        image.entry_offset > std::numeric_limits<std::uint64_t>::max() - text->vm_address) {
        throw macho::FormatError("LC_MAIN entry outside executable bytes");
    }
    const auto entry = text->vm_address + image.entry_offset;
    if (!draft.fetch(entry)) throw macho::FormatError("LC_MAIN entry is not executable");
    memory = std::move(draft);
    return {entry, count};
}
}
