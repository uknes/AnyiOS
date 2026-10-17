#pragma once

#include <anyios/foundation_string.hpp>
#include <anyios/guest_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

namespace anyios::darwin {

// Diagnostic-only, guest-addressed HIGH-LEVEL EMULATION prototype for a string
// object with immutable UTF-16 payload and controlled ownership.
// The private class token is NOT Apple's NSString Class and must NEVER be
// returned through NSStringFromClass, objc_msgSend, UIKit or other guest APIs.
// This primitive intentionally does not claim a registered Objective-C class,
// dispatch table, autorelease pool semantics, or CoreFoundation bridge.
class GuestHleFoundationStringObjects final {
public:
    static constexpr std::size_t kArenaBytes = cpu::GuestMemory::ios_page_size;
    static constexpr std::size_t kObjectBytes = 32;
    static constexpr std::size_t kMaxObjects = 256;

    GuestHleFoundationStringObjects(cpu::GuestMemory& memory,
                                    GuestFoundationStringStorage& strings,
                                    std::uint64_t object_base,
                                    std::uint64_t private_class_base);

    // Each successful create owns one retain. Addresses are never recycled.
    // Failed UTF-8 or capacity requests NEVER publish an object.
    std::optional<std::uint64_t> create(std::string_view utf8);
    bool retain(std::uint64_t object) noexcept;
    bool release(std::uint64_t object) noexcept;

    bool is_live(std::uint64_t object) const noexcept;
    std::optional<std::uint64_t> class_token(std::uint64_t object) const noexcept;
    std::optional<std::size_t> utf16_length(std::uint64_t object) const noexcept;
    std::optional<std::uint16_t> code_unit(std::uint64_t object,
                                           std::size_t index) const noexcept;
    // Returns nullopt when either object is forged, corrupt, or released.
    std::optional<bool> equals(std::uint64_t left,
                               std::uint64_t right) const noexcept;
    std::size_t live_count() const noexcept { return objects_.size(); }
    std::size_t allocated_count() const noexcept { return next_slot_; }

private:
    struct Entry {
        GuestUtf16Snapshot snapshot;
        std::uint32_t references = 1;
    };
    const Entry* checked(std::uint64_t object) const noexcept;
    cpu::GuestMemory& memory_;
    GuestFoundationStringStorage& strings_;
    std::uint64_t base_;
    std::uint64_t class_base_;
    std::size_t next_slot_ = 0;
    std::map<std::uint64_t, Entry> objects_;
};

} // namespace anyios::darwin
