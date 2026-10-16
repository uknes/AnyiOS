#pragma once

#include <anyios/abi_thunk.hpp>
#include <anyios/objc_identity.hpp>
#include <anyios/objc_objects.hpp>
#include <anyios/objc_registry.hpp>
#include <anyios/objc_selectors.hpp>

#include <cstdint>
#include <optional>

namespace anyios::darwin {

// Resolve only +class on a validated compiler-local class receiver and a
// registered compiler-emitted selector. No metaclass dispatch, +initialize,
// method forwarding, NSObject runtime or guest method invocation is implied.
// This models the documented +[NSObject class] identity for the narrow probe.
std::optional<std::uint64_t> resolve_local_class_identity(
    const ObjcIdentityProbe& metadata, const GuestObjcClassRegistry& classes,
    GuestObjcSelectorRegistry& selectors, std::uint64_t guest_receiver,
    std::uint64_t guest_selector);


// Strictly the existing four-register BOOL AppDelegate diagnostic. The
// original ARM64 guest IMP executes through CpuBackend, never as host code.
// This is NOT general _objc_msgSend, UIKit lifecycle, or message forwarding.
std::optional<std::uint64_t> invoke_owned_bool_launch(
    cpu::CpuBackend& backend, const ObjcIdentityProbe& metadata,
    const GuestObjcClassRegistry& classes,
    GuestObjcSelectorRegistry& selectors,
    const GuestObjcObjectArena& objects,
    std::uint64_t guest_receiver, std::uint64_t guest_selector,
    std::uint64_t return_pc, std::uint64_t instruction_budget);

} // namespace anyios::darwin
