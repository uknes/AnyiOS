#pragma once
#include <anyios/guest_memory.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace anyios::darwin {
// Narrow guest ABI for os_log_create only. Returned tokens are opaque guest
// addresses, NOT host pointers or real Darwin/ObjC logging objects.
class GuestOsLogRegistry {
public:
    static constexpr std::size_t kMaxObjects = 256;
    static constexpr std::size_t kMaxNameBytes = 255;
    static constexpr std::size_t kArenaBytes = 16 * 1024;
    GuestOsLogRegistry(cpu::GuestMemory& memory, std::uint64_t guest_base);
    std::optional<std::uint64_t> create(std::uint64_t subsystem, std::uint64_t category);
    bool owns(std::uint64_t token) const noexcept;
    std::size_t size() const noexcept { return entries_.size(); }
private:
    struct Entry { std::string subsystem, category; };
    std::optional<std::string> read_guest_string(std::uint64_t pointer) const;
    cpu::GuestMemory& memory_;
    std::uint64_t base_;
    std::vector<Entry> entries_;
};
}
