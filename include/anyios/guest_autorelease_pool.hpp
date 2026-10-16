#pragma once

#include <anyios/guest_memory.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace anyios::darwin {

// Research-only Objective-C autorelease pool ABI for owned and source-vetted
// guest entry probes. Supports EMPTY nested pools only: no ObjC retain,
// autorelease, release, destructors, or actual Foundation behavior.
// Tokens are unique, guest-memory-backed, read-only opaque capabilities.
// The caller must serialize operations on the emulated guest CPU.
class GuestAutoreleasePools final {
public:
    static constexpr std::size_t kArenaBytes = cpu::GuestMemory::ios_page_size;
    static constexpr std::size_t kMaxTokens = kArenaBytes / 16;
    static constexpr std::size_t kMaxDepth = 16;
    static constexpr std::size_t kMaxThreads = 64;

    GuestAutoreleasePools(cpu::GuestMemory& memory, std::uint64_t guest_base);

    // x0 result of objc_autoreleasePoolPush; failure means unsupported.
    // Tokens are never recycled (including after a successful pop).
    std::optional<std::uint64_t> push(std::uint64_t guest_thread_id);

    // x0 input to objc_autoreleasePoolPop. Only same-thread LIFO is valid.
    // Forged, stale, repeated and out-of-order tokens leave state unchanged.
    bool pop(std::uint64_t guest_thread_id, std::uint64_t token) noexcept;

    bool has_open_pools(std::uint64_t guest_thread_id) const noexcept;
    // Only empty stacks can end; guest object teardown is NOT supported.
    bool finish_thread(std::uint64_t guest_thread_id) const noexcept {
        return !has_open_pools(guest_thread_id);
    }
    std::size_t allocated_tokens() const noexcept { return next_slot_; }

private:
    std::uint64_t base_;
    std::size_t next_slot_ = 0;
    std::map<std::uint64_t, std::vector<std::uint64_t>> stacks_;
};

} // namespace anyios::darwin
