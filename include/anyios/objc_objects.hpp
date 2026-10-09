#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/objc_identity.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace anyios::darwin {

// Single-guest-thread, bounded ownership for instances of validated local
// Clang ObjC classes. This is NOT a complete libobjc allocator or ARC runtime.
// Released guest addresses are deliberately never reused in this probe arena,
// so a stale pointer cannot alias a newly constructed object.
class GuestObjcObjectArena {
public:
    GuestObjcObjectArena(cpu::GuestMemory& memory,
                         std::uint64_t base, std::size_t size);

    // Allocates zeroed guest bytes of class_ro_t instanceSize (minimum 8),
    // 16-byte aligned; installs the original guest Class as isa.
    std::optional<std::uint64_t> allocate(const ObjcIdentityProbe& classes,
                                          std::uint64_t guest_class);
    bool retain(std::uint64_t guest_instance);
    bool release(std::uint64_t guest_instance);
    bool is_live(std::uint64_t guest_instance) const;
    // A live object is valid only while its original guest isa matches
    // the validated class recorded when the object was allocated.
    std::optional<std::uint64_t> class_of(std::uint64_t guest_instance) const;
    std::size_t live_count() const noexcept { return instances_.size(); }

private:
    struct Instance {
        std::size_t allocated_bytes = 0;
        std::uint32_t references = 1;
        std::uint64_t guest_class = 0;
    };

    cpu::GuestMemory& memory_;
    std::uint64_t base_;
    std::size_t capacity_;
    std::size_t used_ = 0;
    std::map<std::uint64_t, Instance> instances_;
};

} // namespace anyios::darwin
