#pragma once

#include <anyios/objc_identity.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace anyios::darwin {

// Interning of compiler-emitted selector names at original guest addresses.
// Does not implement public sel_registerName, dyld selrefs fixups, category
// registration, cross-module selector identity or general objc_msgSend.
class GuestObjcSelectorRegistry {
public:
    explicit GuestObjcSelectorRegistry(const ObjcIdentityProbe& metadata)
        : metadata_(metadata) {}

    // Returns a canonical *guest* address only for a validated ObjC
    // __objc_methname string. Never returns or constructs a host pointer.
    std::optional<std::uint64_t> intern_compiled_selector(
        std::uint64_t guest_selector);

    std::size_t size() const noexcept { return selectors_.size(); }

private:
    const ObjcIdentityProbe& metadata_;
    std::map<std::string, std::uint64_t, std::less<>> selectors_;
};

} // namespace anyios::darwin
