#pragma once
#include <anyios/guest_memory.hpp>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
namespace anyios::darwin {
// Darwin guest getenv: returned pointer always belongs to guest envp memory.
// optional(0) = well-formed lookup miss; nullopt = invalid guest memory or name.
class GuestEnvironment {
public:
    static constexpr std::size_t kMaxEntries = 32;
    static constexpr std::size_t kMaxNameBytes = 255;
    static constexpr std::size_t kMaxEntryBytes = 1024;
    GuestEnvironment(const cpu::GuestMemory& memory, std::uint64_t envp)
        : memory_(memory), envp_(envp) {}
    std::optional<std::uint64_t> lookup(std::uint64_t name) const;
private:
    std::optional<std::string> guest_name(std::uint64_t address) const;
    const cpu::GuestMemory& memory_;
    std::uint64_t envp_;
};
} // namespace anyios::darwin
