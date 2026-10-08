#include <anyios/objc_registry.hpp>

#include <stdexcept>

namespace anyios::darwin {

GuestObjcClassRegistry::GuestObjcClassRegistry(
    const ObjcIdentityProbe& metadata, const cpu::GuestMemory& memory)
    : memory_(memory) {
    const auto classes = metadata.local_classes();
    if (classes.size() > 512) {
        throw std::invalid_argument("too many ObjC local classes");
    }
    for (const auto cls : classes) {
        const auto name = metadata.local_class_name(cls);
        const auto size = metadata.local_instance_size(cls);
        if (!name || !size) {
            throw std::invalid_argument("invalid local Objective-C class record");
        }
        const auto superclass = memory_.read(cls + 8, 8);
        if (!superclass) {
            throw std::invalid_argument("unreadable local Objective-C superclass");
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

} // namespace anyios::darwin
