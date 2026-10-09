#pragma once

#include <anyios/dyld_plan.hpp>
#include <anyios/guest_memory.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace anyios::loader {

struct MappedInitializerImage {
    dyld::Module module;
    std::uint64_t guest_text_base;
};

struct InitializerCall {
    std::string module_path;
    std::uint64_t guest_function;
};

// A bounded, acyclic, completely supplied owned-image graph. Only plans calls:
// it does not map images, execute callbacks, initialize ObjC/Swift or publish
// a loaded namespace. The returned plan owns its path/address snapshots.
std::vector<InitializerCall> plan_owned_image_initializers(
    std::span<const MappedInitializerImage> images,
    std::string_view executable_path,
    const cpu::GuestMemory& memory);

}
