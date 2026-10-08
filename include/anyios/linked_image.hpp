#pragma once

#include <anyios/guest_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace anyios::loader {

struct LinkedImageOptions {
    bool require_ios_pages = false;
    cpu::GuestMemory::MappingJournal* transaction = nullptr;
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

}
