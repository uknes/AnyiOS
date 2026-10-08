#include <anyios/objc_registry.hpp>

#include <set>
#include <stdexcept>

namespace anyios::darwin {

GuestObjcClassRegistry::GuestObjcClassRegistry(
    const ObjcIdentityProbe& metadata, const cpu::GuestMemory& memory)
    : metadata_(metadata), memory_(memory) {
    const auto classes = metadata.local_classes();
    if (classes.size() > 512) {
        throw std::invalid_argument("too many ObjC local classes");
    }
    for (const auto cls : classes) {
        const auto name = metadata.local_class_name(cls);
        const auto size = metadata.local_instance_size(cls);
        const auto superclass = memory_.read(cls + 8, 8);
        // Preserve independent valid classes when a separate local record has
        // an unsupported class_ro_t layout. Do not fabricate such classes:
        // only validated records are published for lookup and dispatch.
        if (!name || !size || !superclass) {
            ++unresolved_count_;
            continue;
        }
        if (!by_name_.emplace(*name, cls).second ||
            !by_address_.emplace(cls, *superclass).second) {
            throw std::invalid_argument("duplicate local Objective-C class");
        }
    }
}

std::optional<std::string> GuestObjcClassRegistry::checked_guest_name(
    std::uint64_t address) const {
    if (!address) return std::nullopt;
    std::string result;
    for (std::size_t i = 0; i < 128; ++i) {
        // GuestMemory::read checks mapped read permissions and overflow.
        if (address > UINT64_MAX - i) return std::nullopt;
        const auto ch = memory_.read(address + i, 1);
        if (!ch) return std::nullopt;
        if (*ch == 0) {
            if (!result.empty()) return result;
            return std::nullopt;
        }
        if (*ch < 33 || *ch > 126) return std::nullopt;
        result.push_back(static_cast<char>(*ch));
    }
    return std::nullopt;
}

std::optional<std::uint64_t> GuestObjcClassRegistry::find(
    std::string_view name) const {
    const auto it = by_name_.find(name);
    if (it == by_name_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::uint64_t> GuestObjcClassRegistry::find_guest_name(
    std::uint64_t guest_cstring) const {
    const auto name = checked_guest_name(guest_cstring);
    if (!name) return std::nullopt;
    return find(*name);
}

std::optional<std::uint64_t> GuestObjcClassRegistry::local_superclass(
    std::uint64_t local_class) const {
    const auto it = by_address_.find(local_class);
    if (it == by_address_.end()) return std::nullopt;
    const auto superclass = it->second;
    if (superclass == 0 || by_address_.contains(superclass)) {
        return superclass;
    }
    // Superclass may refer to a yet-unimplemented framework class.
    return std::nullopt;
}

std::optional<GuestObjcMethod> GuestObjcClassRegistry::resolve_local_instance_method(
    std::uint64_t local_class, std::string_view selector) const {
    if (selector.empty() || !by_address_.contains(local_class)) {
        return std::nullopt;
    }
    std::set<std::uint64_t> visited;
    auto current = local_class;
    while (current != 0 && visited.size() < 64) {
        if (!visited.insert(current).second) {
            return std::nullopt; // Corrupt local superclass cycle.
        }
        if (const auto method = metadata_.local_instance_method(current, selector)) {
            return method;
        }
        const auto parent = local_superclass(current);
        if (!parent) {
            return std::nullopt; // External or unresolved superclass.
        }
        current = *parent;
    }
    return std::nullopt;
}

} // namespace anyios::darwin
