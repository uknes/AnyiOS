#pragma once

#include <anyios/cpu_backend.hpp>
#include <anyios/objc_identity.hpp>
#include <anyios/objc_objects.hpp>
#include <anyios/objc_registry.hpp>
#include <anyios/objc_selectors.hpp>

#include <cstdint>
#include <optional>

namespace anyios::darwin {

// Invoke only a validated local instance's fixed four-register BOOL method.
// The two UIKit-related id arguments MUST be nil: this is an inspected
// diagnostic, not UIApplication lifecycle dispatch or generic objc_msgSend.
// Nullopt denotes unsupported dispatch; a guest CPU fault propagates.
std::optional<std::uint64_t> invoke_guest_bool_launch_message(
    cpu::CpuBackend& backend, const cpu::GuestMemory& memory,
    const ObjcIdentityProbe& metadata, const GuestObjcClassRegistry& classes,
    GuestObjcSelectorRegistry& selectors, const GuestObjcObjectArena& objects,
    std::uint64_t receiver, std::uint64_t guest_selector,
    std::uint64_t application, std::uint64_t launch_options,
    std::uint64_t return_pc, std::uint64_t instruction_budget);

} // namespace anyios::darwin
