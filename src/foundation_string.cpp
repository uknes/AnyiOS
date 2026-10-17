#include <anyios/foundation_string.hpp>
#include <anyios/objc_identity.hpp>
#include <anyios/objc_registry.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace anyios::darwin {
namespace {
// Strict Unicode scalar decoder. No normalization or lossy replacement.
std::optional<std::u16string> to_utf16(std::string_view utf8) {
    std::u16string result;
    for (std::size_t i = 0; i < utf8.size();) {
        const auto lead = static_cast<std::uint8_t>(utf8[i]);
        std::uint32_t scalar = 0;
        std::size_t bytes = 0;
        std::uint32_t minimum = 0;
        if (lead < 0x80U) {
            scalar = lead;
            bytes = 1;
        } else if (lead >= 0xC2U && lead <= 0xDFU) {
            scalar = lead & 0x1fU;
            bytes = 2;
            minimum = 0x80;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            scalar = lead & 0x0fU;
            bytes = 3;
            minimum = 0x800;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            scalar = lead & 0x07U;
            bytes = 4;
            minimum = 0x10000;
        } else {
            return std::nullopt;
        }
        if (bytes > utf8.size() - i) return std::nullopt;
        for (std::size_t j = 1; j < bytes; ++j) {
            const auto ch = static_cast<std::uint8_t>(utf8[i + j]);
            if ((ch & 0xC0U) != 0x80U) return std::nullopt;
            scalar = (scalar << 6U) | (ch & 0x3fU);
        }
        if (scalar < minimum || scalar > 0x10FFFFU ||
            (scalar >= 0xD800U && scalar <= 0xDFFFU)) {
            return std::nullopt;
        }
        const auto units = scalar >= 0x10000U ? 2U : 1U;
        if (result.size() > GuestFoundationStringStorage::kMaxCodeUnits - units) {
            return std::nullopt;
        }
        if (units == 1) {
            result.push_back(static_cast<char16_t>(scalar));
        } else {
            scalar -= 0x10000U;
            result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10U)));
            result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3ffU)));
        }
        i += bytes;
    }
    return result;
}
} // namespace

GuestFoundationStringStorage::GuestFoundationStringStorage(
    cpu::GuestMemory& memory, std::uint64_t guest_base)
    : memory_(memory), base_(guest_base) {
    if (!memory_.map_ios(base_, kArenaBytes, cpu::bits(cpu::Access::read))) {
        throw std::runtime_error("guest UTF-16 snapshot arena mapping refused");
    }
}

std::optional<GuestUtf16Snapshot> GuestFoundationStringStorage::stage_utf8(
    std::string_view utf8) {
    const auto utf16 = to_utf16(utf8);
    if (!utf16) return std::nullopt;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        if (entries_[index].utf8 == utf8) {
            return GuestUtf16Snapshot{
                base_ + static_cast<std::uint64_t>(index * kStride),
                entries_[index].length
            };
        }
    }
    if (entries_.size() >= kMaxEntries) return std::nullopt;

    // Explicit little-endian UTF-16 code units, not host wchar_t or
    // architecture-dependent char16_t memory representation.
    std::array<std::byte, kStride> buffer{};
    for (std::size_t index = 0; index < utf16->size(); ++index) {
        const auto unit = static_cast<std::uint16_t>((*utf16)[index]);
        buffer[2 * index] = std::byte{static_cast<unsigned char>(unit & 0xffU)};
        buffer[2 * index + 1] = std::byte{static_cast<unsigned char>(unit >> 8U)};
    }
    const auto guest_data = base_ +
        static_cast<std::uint64_t>(entries_.size() * kStride);
    if (!memory_.load(guest_data, buffer)) return std::nullopt;
    // Publish only after the bounded guest content is completely staged.
    entries_.push_back({std::string(utf8), utf16->size()});
    return GuestUtf16Snapshot{guest_data, utf16->size()};
}

std::optional<std::size_t> GuestFoundationStringStorage::length(
    std::uint64_t guest_data) const noexcept {
    if (guest_data < base_) return std::nullopt;
    const auto offset = guest_data - base_;
    if (offset % kStride != 0 || offset / kStride >= entries_.size()) {
        return std::nullopt;
    }
    return entries_[static_cast<std::size_t>(offset / kStride)].length;
}

std::optional<std::uint16_t> GuestFoundationStringStorage::code_unit(
    std::uint64_t guest_data, std::size_t index) const noexcept {
    const auto count = length(guest_data);
    if (!count || index >= *count) return std::nullopt;
    return static_cast<std::uint16_t>(
        *memory_.read(guest_data + static_cast<std::uint64_t>(index * 2), 2));
}

ClassNamePreflight preflight_foundation_class_name(
    const ObjcIdentityProbe& metadata, const GuestObjcClassRegistry& classes,
    GuestFoundationStringStorage& strings, std::uint64_t guest_class) {
    if (!guest_class) return {ClassNamePreflightStatus::nil_class, std::nullopt};
    const auto name = metadata.local_class_name(guest_class);
    if (!name || classes.find(*name) != guest_class) {
        return {ClassNamePreflightStatus::unsupported_class_or_string, std::nullopt};
    }
    const auto snapshot = strings.stage_utf8(*name);
    if (!snapshot) {
        return {ClassNamePreflightStatus::unsupported_class_or_string, std::nullopt};
    }
    return {ClassNamePreflightStatus::valid_utf16_snapshot, snapshot};
}

} // namespace anyios::darwin
