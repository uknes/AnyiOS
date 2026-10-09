#pragma once

#include <anyios/guest_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace anyios::loader {

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
