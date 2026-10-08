#pragma once

#include <anyios/guest_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace anyios::loader {

struct LoadedExecutable {
    std::uint64_t entry_address;
    std::size_t mapped_segments;
};

LoadedExecutable load_static_executable(std::span<const std::byte> file,
                                        anyios::cpu::GuestMemory& memory);

}
