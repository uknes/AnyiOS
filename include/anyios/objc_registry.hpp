#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/objc_identity.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace anyios::darwin {

// A validated index of *local* Clang-emitted ObjC class metadata. This is not
// libobjc: unresolved parent classes, imported framework classes, metaclasses,
// class initialization, categories, dynamic registration and dispatch are
// deliberately not represented as working.
class GuestObjcClassRegistry {
public:
    GuestObjcClassRegistry(const ObjcIdentityProbe& metadata,
                           const cpu::GuestMemory& memory);

    std::optional<std::uint64_t> find(std::string_view name) const;
    // Models objc_getClass's C-string input only for registered local classes.
    // Invalid/nonterminated name strings fail closed instead of dereferencing
    // host pointers. A missing class returns nullopt, not a fabricated class.
    std::optional<std::uint64_t> find_guest_name(
        std::uint64_t guest_cstring) const;

    // Zero is an explicitly local root class. An outside-framework or malformed
    // superclass is unresolved (nullopt), never silently treated as root.
    std::optional<std::uint64_t> local_superclass(
        std::uint64_t local_class) const;

    std::size_t size() const noexcept { return by_name_.size(); }

private:
    const cpu::GuestMemory& memory_;
    std::map<std::string, std::uint64_t, std::less<>> by_name_;
    std::map<std::uint64_t, std::uint64_t> by_address_;

    std::optional<std::string> checked_guest_name(
        std::uint64_t address) const;
};

} // namespace anyios::darwin
