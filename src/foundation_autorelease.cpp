#include <anyios/foundation_autorelease.hpp>

namespace anyios::darwin {

std::optional<std::uint64_t> GuestFoundationAutoreleaseScopes::push(
    std::uint64_t guest_thread) {
    const auto token = pools_.push(guest_thread);
    if (!token) return std::nullopt;
    frames_[guest_thread].push_back(Frame{*token, {}});
    return token;
}

bool GuestFoundationAutoreleaseScopes::enqueue(
    std::uint64_t guest_thread, std::uint64_t hle_object) {
    const auto thread = frames_.find(guest_thread);
    if (thread == frames_.end() || thread->second.empty() ||
        total_pending_ >= kMaxPending) {
        return false;
    }
    const auto count = objects_.reference_count(hle_object);
    if (!count || *count == 0) return false;
    const auto reservation = reservations_.find(hle_object);
    const auto used = reservation == reservations_.end() ?
        std::size_t{0} : reservation->second;
    if (used >= *count) return false;

    thread->second.back().pending.push_back(hle_object);
    reservations_[hle_object] = used + 1;
    ++total_pending_;
    return true;
}

bool GuestFoundationAutoreleaseScopes::pop(
    std::uint64_t guest_thread, std::uint64_t token) {
    const auto thread = frames_.find(guest_thread);
    if (thread == frames_.end() || thread->second.empty() ||
        thread->second.back().token != token) {
        return false;
    }
    const auto& frame = thread->second.back();
    // An external release, corrupted isa, or stale object blocks the entire
    // drain. Never silently accept a dangling object or underflow a retain.
    for (const auto object : frame.pending) {
        const auto count = objects_.reference_count(object);
        const auto reserved = reservations_.find(object);
        if (!count || reserved == reservations_.end() ||
            reserved->second == 0 || *count < reserved->second) {
            return false;
        }
    }
    // Atomic under the single-thread diagnostic model: pool token consumed
    // only after all guest objects and reference counts were prevalidated.
    if (!pools_.pop(guest_thread, token)) return false;
    for (auto it = frame.pending.rbegin(); it != frame.pending.rend(); ++it) {
        if (!objects_.release(*it)) return false;
        auto reserved = reservations_.find(*it);
        if (--reserved->second == 0) reservations_.erase(reserved);
        --total_pending_;
    }
    thread->second.pop_back();
    if (thread->second.empty()) frames_.erase(thread);
    return true;
}

bool GuestFoundationAutoreleaseScopes::finish_thread(
    std::uint64_t guest_thread) const noexcept {
    const auto thread = frames_.find(guest_thread);
    return (thread == frames_.end() || thread->second.empty()) &&
           pools_.finish_thread(guest_thread);
}

} // namespace anyios::darwin
