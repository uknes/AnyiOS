#pragma once

#include <anyios/guest_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace anyios::darwin {
class ObjcIdentityProbe;
class GuestObjcClassRegistry;

// Guest-readable UTF-16LE *character storage*, NOT an NSString or CFString.
// In particular, its address MUST NEVER be passed as an Objective-C object.
// Later Foundation work must create and manage real HLE object identity,
// lifecycle, method dispatch, and NSString/CFString bridge semantics separately.
struct GuestUtf16Snapshot {
    std::uint64_t guest_data = 0;
    std::size_t code_units = 0;
};

class GuestFoundationStringStorage final {
public:
    static constexpr std::size_t kArenaBytes = cpu::GuestMemory::ios_page_size;
    static constexpr std::size_t kStride = 512;
    static constexpr std::size_t kMaxEntries = kArenaBytes / kStride;
    static constexpr std::size_t kMaxCodeUnits = kStride / 2 - 1;

    GuestFoundationStringStorage(cpu::GuestMemory& memory, std::uint64_t guest_base);

    // Validates well-formed UTF-8 without replacement; rejects surrogate
    // encodings, overlong sequences, non-Unicode scalars and excess UTF-16
    // capacity. Empty string is a valid snapshot. Guest data is immutable
    // (never executable or guest-writable), terminated with U+0000.
    std::optional<GuestUtf16Snapshot> stage_utf8(std::string_view utf8);

    // Only exact live snapshot addresses can be queried; guest interior
    // pointers, forged pointers, and past-end reads fail closed.
    std::optional<std::size_t> length(std::uint64_t guest_data) const noexcept;
    std::optional<std::uint16_t> code_unit(
        std::uint64_t guest_data, std::size_t index) const noexcept;
    std::size_t size() const noexcept { return entries_.size(); }

private:
    struct Entry {
        std::string utf8;
        std::size_t length;
    };
    cpu::GuestMemory& memory_;
    std::uint64_t base_;
    std::vector<Entry> entries_;
};

enum class ClassNamePreflightStatus {
    nil_class,
    valid_utf16_snapshot,
    unsupported_class_or_string
};

struct ClassNamePreflight {
    ClassNamePreflightStatus status = ClassNamePreflightStatus::unsupported_class_or_string;
    std::optional<GuestUtf16Snapshot> snapshot;
};

// This validates class-to-name/UTF-16 semantics ONLY; does not implement
// NSStringFromClass's Objective-C return object. Unsupported classes must not
// be confused with the documented nil-class => nil return.
ClassNamePreflight preflight_foundation_class_name(
    const ObjcIdentityProbe& metadata, const GuestObjcClassRegistry& classes,
    GuestFoundationStringStorage& strings, std::uint64_t guest_class);

} // namespace anyios::darwin
