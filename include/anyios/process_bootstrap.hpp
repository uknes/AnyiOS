#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace anyios::loader {

struct AppleProcessStart {
    std::uint64_t sp = 0;
    std::uint64_t argc = 0;
    std::uint64_t argv = 0;
    std::uint64_t envp = 0;
    std::uint64_t apple = 0;
};

// Builds a bounded, argv/envp/apple vector frame above a 16-byte-aligned
// guest SP. This describes the project-owned LC_MAIN test contract, not dyld.
AppleProcessStart prepare_owned_process_stack(
    cpu::GuestMemory& memory, std::uint64_t stack_base,
    std::size_t stack_size,
    std::span<const std::string_view> arguments,
    std::span<const std::string_view> environment,
    std::span<const std::string_view> apple);

std::vector<std::uint64_t> find_owned_module_initializers(
    const macho::Image& image, const cpu::GuestMemory& memory,
    std::uint64_t guest_image_base);

}
