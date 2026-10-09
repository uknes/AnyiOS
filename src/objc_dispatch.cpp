#include <anyios/objc_dispatch.hpp>
#include <anyios/objc_signature.hpp>

#include <array>
#include <string_view>

namespace anyios::darwin {

std::optional<std::uint64_t> invoke_owned_bool_launch(
    cpu::CpuBackend& backend, const ObjcIdentityProbe& metadata,
    const GuestObjcClassRegistry& classes,
    GuestObjcSelectorRegistry& selectors,
    const GuestObjcObjectArena& objects,
    std::uint64_t guest_receiver, std::uint64_t guest_selector,
    std::uint64_t return_pc, std::uint64_t instruction_budget) {
    // Prevent forged, released, or corrupted guest isa receivers.
    const auto isa = objects.class_of(guest_receiver);
    if (!isa) return std::nullopt;
    const auto name = metadata.local_class_name(*isa);
    if (!name || classes.find(*name) != isa) return std::nullopt;

    const auto canonical = selectors.intern_compiled_selector(guest_selector);
    if (!canonical) return std::nullopt;
    const auto selector_name = metadata.selector_name(*canonical);
    if (selector_name != "application:didFinishLaunchingWithOptions:") {
        return std::nullopt;
    }
    const auto method = classes.resolve_local_instance_method(
        *isa, *selector_name);
    if (!method || !method->type_encoding ||
        !supported_bool_launch_abi(*method->type_encoding)) {
        return std::nullopt;
    }
    // Only nil UIApplication and launch options are known valid for this
    // inspected pinned callback. Real UIKit parameters are NOT implemented.
    const std::array<std::uint64_t, 4> args{
        guest_receiver, *canonical, 0, 0
    };
    const auto value = abi::invoke_guest_callback(
        backend, method->entry, args, return_pc, instruction_budget);
    // For the strictly known bool-return signature, reject other values.
    if (value != 0 && value != 1) return std::nullopt;
    return value;
}

} // namespace anyios::darwin
