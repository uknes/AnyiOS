#include <anyios/foundation_objects.hpp>

#include <array>
#include <limits>
#include <stdexcept>

namespace anyios::darwin {
namespace {
constexpr std::uint64_t kPrivateClassMarker = 0x31454c482d534f49ULL;
constexpr std::uint64_t kObjectGuard = 0x31474e525453454cULL;

template<std::size_t N>
void put64(std::array<std::byte, N>& out, std::size_t offset,
           std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        out[offset + i] = std::byte{static_cast<unsigned char>(
            (value >> (8 * i)) & 0xffU)};
    }
}
} // namespace

GuestHleFoundationStringObjects::GuestHleFoundationStringObjects(
    cpu::GuestMemory& memory, GuestFoundationStringStorage& strings,
    std::uint64_t object_base, std::uint64_t private_class_base)
    : memory_(memory), strings_(strings),
      base_(object_base), class_base_(private_class_base) {
    // The token page contains an HLE marker, NOT a real objc_class.
    // If one mapping fails, roll back the other; no half-initialized arena.
    cpu::GuestMemory::MappingJournal journal(memory_);
    constexpr auto rw = cpu::bits(cpu::Access::read) |
                        cpu::bits(cpu::Access::write);
    if (!journal.map(base_, kArenaBytes, rw, true) ||
        !journal.map(class_base_, kArenaBytes,
                     cpu::bits(cpu::Access::read), true)) {
        throw std::invalid_argument("HLE string object/class arena conflicts with mapping");
    }
    std::array<std::byte, 8> marker{};
    put64(marker, 0, kPrivateClassMarker);
    if (!journal.load(class_base_, marker)) {
        throw std::invalid_argument("HLE private class marker could not be staged");
    }
    journal.commit();
}

const GuestHleFoundationStringObjects::Entry*
GuestHleFoundationStringObjects::checked(std::uint64_t object) const noexcept {
    const auto it = objects_.find(object);
    if (it == objects_.end()) return nullptr;
    const auto isa = memory_.read(object, 8);
    const auto data = memory_.read(object + 8, 8);
    const auto length = memory_.read(object + 16, 8);
    const auto guard = memory_.read(object + 24, 8);
    const auto marker = memory_.read(class_base_, 8);
    if (isa != class_base_ || data != it->second.snapshot.guest_data ||
        length != it->second.snapshot.code_units || guard != kObjectGuard ||
        marker != kPrivateClassMarker ||
        strings_.length(it->second.snapshot.guest_data) !=
            it->second.snapshot.code_units) {
        return nullptr;
    }
    return &it->second;
}

std::optional<std::uint64_t> GuestHleFoundationStringObjects::create(
    std::string_view utf8) {
    if (next_slot_ >= kMaxObjects) return std::nullopt;
    const auto snapshot = strings_.stage_utf8(utf8);
    if (!snapshot) return std::nullopt;
    const auto object = base_ + static_cast<std::uint64_t>(
        next_slot_ * kObjectBytes);
    std::array<std::byte, kObjectBytes> header{};
    put64(header, 0, class_base_);
    put64(header, 8, snapshot->guest_data);
    put64(header, 16, static_cast<std::uint64_t>(snapshot->code_units));
    put64(header, 24, kObjectGuard);
    if (!memory_.copy_to(object, header)) return std::nullopt;
    objects_.emplace(object, Entry{*snapshot, 1});
    ++next_slot_;
    return object;
}

bool GuestHleFoundationStringObjects::retain(std::uint64_t object) noexcept {
    if (!checked(object)) return false;
    auto& entry = objects_.at(object);
    if (entry.references == std::numeric_limits<std::uint32_t>::max()) return false;
    ++entry.references;
    return true;
}

bool GuestHleFoundationStringObjects::release(std::uint64_t object) noexcept {
    if (!checked(object)) return false;
    auto it = objects_.find(object);
    if (it->second.references > 1) {
        --it->second.references;
        return true;
    }
    const std::array<std::byte, kObjectBytes> cleared{};
    if (!memory_.copy_to(object, cleared)) return false;
    objects_.erase(it);
    return true;
}

bool GuestHleFoundationStringObjects::is_live(std::uint64_t object) const noexcept {
    return checked(object) != nullptr;
}

std::optional<std::uint32_t> GuestHleFoundationStringObjects::reference_count(
    std::uint64_t object) const noexcept {
    const auto entry = checked(object);
    return entry ? std::optional<std::uint32_t>{entry->references}
                 : std::nullopt;
}

std::optional<std::uint64_t> GuestHleFoundationStringObjects::class_token(
    std::uint64_t object) const noexcept {
    return checked(object) ? std::optional<std::uint64_t>{class_base_}
                           : std::nullopt;
}

std::optional<std::size_t> GuestHleFoundationStringObjects::utf16_length(
    std::uint64_t object) const noexcept {
    const auto entry = checked(object);
    return entry ? std::optional<std::size_t>{entry->snapshot.code_units}
                 : std::nullopt;
}

std::optional<std::uint16_t> GuestHleFoundationStringObjects::code_unit(
    std::uint64_t object, std::size_t index) const noexcept {
    const auto entry = checked(object);
    if (!entry) return std::nullopt;
    return strings_.code_unit(entry->snapshot.guest_data, index);
}

std::optional<bool> GuestHleFoundationStringObjects::equals(
    std::uint64_t left, std::uint64_t right) const noexcept {
    const auto a = checked(left);
    const auto b = checked(right);
    if (!a || !b) return std::nullopt;
    if (a->snapshot.code_units != b->snapshot.code_units) return false;
    for (std::size_t i = 0; i < a->snapshot.code_units; ++i) {
        const auto aa = strings_.code_unit(a->snapshot.guest_data, i);
        const auto bb = strings_.code_unit(b->snapshot.guest_data, i);
        if (!aa || !bb) return std::nullopt;
        if (*aa != *bb) return false;
    }
    return true;
}

} // namespace anyios::darwin
