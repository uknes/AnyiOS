#include <anyios/objc_objects.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

namespace anyios::darwin {
GuestObjcObjectArena::GuestObjcObjectArena(
    cpu::GuestMemory& memory, std::uint64_t base, std::size_t size)
    : memory_(memory), base_(base), capacity_(size) {
    constexpr auto rw = cpu::bits(cpu::Access::read) |
                        cpu::bits(cpu::Access::write);
    if (size == 0 || size > 1024 * 1024 ||
        base > std::numeric_limits<std::uint64_t>::max() - size ||
        !memory_.map_ios(base, size, rw)) {
        throw std::invalid_argument("cannot reserve bounded ObjC guest object arena");
    }
}

std::optional<std::uint64_t> GuestObjcObjectArena::allocate(
    const ObjcIdentityProbe& classes, std::uint64_t guest_class) {
    const auto size = classes.local_instance_size(guest_class);
    if (!size || instances_.size() >= 256) return std::nullopt;
    // local_instance_size is bounded to [8, 4096], so alignment cannot overflow.
    const auto aligned = (static_cast<std::size_t>(*size) + 15U) & ~std::size_t{15};
    if (used_ > capacity_ || aligned > capacity_ - used_) return std::nullopt;
    const auto address = base_ + used_;
    std::vector<std::byte> zeros(aligned, std::byte{0});
    if (!memory_.load(address, zeros) ||
        !memory_.write(address, guest_class, 8)) {
        return std::nullopt;
    }
    // Commit the allocation only after all guest-memory operations succeed.
    instances_.emplace(address, Instance{aligned, 1, guest_class});
    used_ += aligned;
    return address;
}

bool GuestObjcObjectArena::retain(std::uint64_t guest_instance) {
    const auto it = instances_.find(guest_instance);
    if (it == instances_.end() ||
        it->second.references == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    ++it->second.references;
    return true;
}

bool GuestObjcObjectArena::release(std::uint64_t guest_instance) {
    const auto it = instances_.find(guest_instance);
    if (it == instances_.end()) return false;
    if (it->second.references > 1) {
        --it->second.references;
        return true;
    }
    // Zero before invalidating the host-owned record; failure is fail-closed.
    const std::vector<std::byte> zeros(it->second.allocated_bytes, std::byte{0});
    if (!memory_.allowed(guest_instance, zeros.size(), cpu::Access::write) ||
        !memory_.load(guest_instance, zeros)) {
        return false;
    }
    instances_.erase(it);
    return true;
}

bool GuestObjcObjectArena::is_live(std::uint64_t guest_instance) const {
    return instances_.contains(guest_instance);
}

std::optional<std::uint64_t> GuestObjcObjectArena::class_of(
    std::uint64_t guest_instance) const {
    const auto it = instances_.find(guest_instance);
    if (it == instances_.end()) return std::nullopt;
    const auto isa = memory_.read(guest_instance, 8);
    if (!isa || *isa != it->second.guest_class) return std::nullopt;
    return isa;
}

} // namespace anyios::darwin
