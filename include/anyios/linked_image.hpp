#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace anyios::loader {

// Shared 16 KiB mapping policy. Only a final read-only __LINKEDIT tail may
// round up; its padding is never an original symbol/initializer target.
std::uint64_t ios_segment_mapping_size(const macho::Image& image, std::size_t index);

struct LinkedImageOptions {
    bool require_ios_pages = false;
    cpu::GuestMemory::MappingJournal* transaction = nullptr;
    bool allow_legacy_fixups = false;
};

struct LinkedImage {
    std::uint64_t guest_base;
    std::uint64_t guest_entry;
    std::size_t segment_count;
    std::size_t patched_pointers;
};

LinkedImage stage_linked_image(
    std::span<const std::byte> file,
    cpu::GuestMemory& memory,
    std::uint64_t guest_base,
    std::span<const std::uint64_t> resolved_imports,
    LinkedImageOptions options = {});

struct LinkedImageInput {
    std::span<const std::byte> file;
    std::uint64_t guest_base;
    std::span<const std::uint64_t> resolved_imports;
    bool allow_legacy_fixups = false;
};

// One transaction for an already resolved set. Every final binding must point
// into an original readable segment in this set. No runtime trap targets.
std::vector<LinkedImage> stage_linked_images(
    std::span<const LinkedImageInput> inputs,
    cpu::GuestMemory& memory,
    bool require_ios_pages = false);

}
