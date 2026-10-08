#pragma once

#include <anyios/guest_memory.hpp>
#include <cstddef>
#include <cstdint>
#include <span>

namespace anyios::loader {

struct OwnedLinkedPair {
    std::uint64_t entry;
    std::uint64_t imported_function;
    std::size_t executable_fixups;
    std::size_t library_fixups;
};

// Restricted to owned iPhoneOS fixtures, not a general dyld implementation.
// Commits both images or leaves guest memory unchanged.
OwnedLinkedPair stage_owned_linked_pair(
    std::span<const std::byte> executable,
    std::span<const std::byte> library,
    cpu::GuestMemory& memory,
    std::uint64_t executable_base,
    std::uint64_t library_base);

}
