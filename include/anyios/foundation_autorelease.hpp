#pragma once

#include <anyios/foundation_objects.hpp>
#include <anyios/guest_autorelease_pool.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace anyios::darwin {

// Research-only private HLE string ownership; NEVER a general libobjc ARC
// runtime. A scope queues exactly one future release of a validated private
// HLE string object. The caller must serialize guest execution.
class GuestFoundationAutoreleaseScopes final {
public:
    static constexpr std::size_t kMaxPending = 256;

    GuestFoundationAutoreleaseScopes(GuestAutoreleasePools& pools,
                                     GuestHleFoundationStringObjects& objects)
        : pools_(pools), objects_(objects) {}

    // Tokens come from the existing bounded, unique guest pool ABI.
    std::optional<std::uint64_t> push(std::uint64_t guest_thread);
    // Does not retain. A queued release must have a valid owned reference.
    // No guest NSString, UIKit, Foundation dispatcher or CFString integration.
    bool enqueue(std::uint64_t guest_thread, std::uint64_t hle_object);
    // Validates all pending references before draining in reverse order.
    // Rejects cross-thread, replayed and out-of-order tokens without drain.
    bool pop(std::uint64_t guest_thread, std::uint64_t token);
    bool finish_thread(std::uint64_t guest_thread) const noexcept;
    std::size_t pending() const noexcept { return total_pending_; }

private:
    struct Frame {
        std::uint64_t token = 0;
        std::vector<std::uint64_t> pending;
    };
    GuestAutoreleasePools& pools_;
    GuestHleFoundationStringObjects& objects_;
    std::map<std::uint64_t, std::vector<Frame>> frames_;
    std::map<std::uint64_t, std::size_t> reservations_;
    std::size_t total_pending_ = 0;
};

} // namespace anyios::darwin
