#include <anyios/guest_autorelease_pool.hpp>

#include <stdexcept>

namespace anyios::darwin {

GuestAutoreleasePools::GuestAutoreleasePools(
    cpu::GuestMemory& memory, std::uint64_t guest_base)
    : base_(guest_base) {
    // map_ios checks alignment, address bounds, collisions and permissions.
    // This page holds opaque tokens, not an autorelease implementation.
    if (!memory.map_ios(base_, kArenaBytes, cpu::bits(cpu::Access::read))) {
        throw std::runtime_error("guest autorelease token arena conflicts with mapping");
    }
}

std::optional<std::uint64_t> GuestAutoreleasePools::push(
    std::uint64_t guest_thread_id) {
    const auto it = stacks_.find(guest_thread_id);
    if (next_slot_ >= kMaxTokens ||
        (it == stacks_.end() && stacks_.size() >= kMaxThreads) ||
        (it != stacks_.end() && it->second.size() >= kMaxDepth)) {
        return std::nullopt;
    }
    const auto token = base_ + static_cast<std::uint64_t>(next_slot_ * 16);
    stacks_[guest_thread_id].push_back(token);
    ++next_slot_;
    return token;
}

bool GuestAutoreleasePools::pop(
    std::uint64_t guest_thread_id, std::uint64_t token) noexcept {
    const auto it = stacks_.find(guest_thread_id);
    if (it == stacks_.end() || it->second.empty() ||
        it->second.back() != token) {
        return false;
    }
    it->second.pop_back();
    if (it->second.empty()) stacks_.erase(it);
    return true;
}

bool GuestAutoreleasePools::has_open_pools(
    std::uint64_t guest_thread_id) const noexcept {
    const auto it = stacks_.find(guest_thread_id);
    return it != stacks_.end() && !it->second.empty();
}

} // namespace anyios::darwin
