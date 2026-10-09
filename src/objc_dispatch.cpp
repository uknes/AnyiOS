#include <anyios/objc_dispatch.hpp>

#include <anyios/abi_thunk.hpp>
#include <anyios/objc_signature.hpp>

#include <array>
#include <string_view>

namespace anyios::darwin {

std::optional<std::uint64_t> invoke_guest_bool_launch_message(
    cpu::CpuBackend& backend, const cpu::GuestMemory& memory,
    const ObjcIdentityProbe& metadata, const GuestObjcClassRegistry& classes,
    GuestObjcSelectorRegistry& selectors, const GuestObjcObjectArena& objects,
    std::uint64_t receiver, std::uint64_t guest_selector,
    std::uint64_t application, std::uint64_t launch_options,
    std::uint64_t return_pc, std::uint64_t instruction_budget) {
    constexpr std::string_view kLaunchSelector =
        "application:didFinishLaunchingWithOptions:";

    // The diagnostic does not supply a genuine UIApplication or options
    // object. Refuse foreign pointers instead of passing them into guest code.
    if (!receiver || !objects.is_live(receiver) ||
        application || launch_options || instruction_budget == 0) {
        return std::nullopt;
    }

    const auto name = metadata.selector_name(guest_selector);
    if (!name || *name != kLaunchSelector) return std::nullopt;

    const auto original_class = memory.read(receiver, 8);
    if (!original_class) return std::nullopt;
    const auto method = classes.resolve_local_instance_method(
        *original_class, *name);
    if (!method || !method->type_encoding ||
        !supported_bool_launch_abi(*method->type_encoding)) {
        return std::nullopt;
    }

    // Method-list SEL may differ from the caller's duplicate compiler string.
    // Intern through the original mapped guest section, never a host string.
    const auto canonical = selectors.intern_compiled_selector(method->selector);
    if (!canonical) return std::nullopt;
    if (metadata.selector_name(*canonical) != *name) return std::nullopt;

    const std::array<std::uint64_t, 4> args{
        receiver, *canonical, application, launch_options
    };
    // This invokes the actual original guest ARM64 IMP under the existing
    // guarded callback ABI. Never dereference the guest IMP as a host pointer.
    const auto value = abi::invoke_guest_callback(
        backend, method->entry, args, return_pc, instruction_budget);
    return value & 0xffU; // BOOL is the low byte of w0 for the allowed ABI.
}

} // namespace anyios::darwin
