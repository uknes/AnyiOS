#include <anyios/objc_selectors.hpp>

namespace anyios::darwin {

std::optional<std::uint64_t> GuestObjcSelectorRegistry::intern_compiled_selector(
    std::uint64_t guest_selector) {
    // ObjcIdentityProbe enforces section residency, guest readability,
    // character constraints, max length and NUL termination.
    const auto name = metadata_.selector_name(guest_selector);
    if (!name) return std::nullopt;

    const auto found = selectors_.find(*name);
    if (found != selectors_.end()) {
        // Recheck that the canonical guest address still spells the original
        // name; rejecting stale metadata is safer than guessing a new pointer.
        if (metadata_.selector_name(found->second) != *name) {
            return std::nullopt;
        }
        return found->second;
    }
    if (selectors_.size() >= 256) return std::nullopt;
    return selectors_.emplace(*name, guest_selector).first->second;
}

} // namespace anyios::darwin
