#pragma once

#include <anyios/darwin_syscall.hpp>
#include <anyios/guest_memory.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string_view>

namespace anyios::darwin {

// A guest abort is a terminal guest event, never a successful return.
struct GuestAbort final : std::runtime_error {
    GuestAbort() : std::runtime_error("owned Darwin guest called abort") {}
};

struct LibSystemCall {
    std::uint64_t value = 0;
    bool exited = false;
};

// Explicit, minimal _malloc/_write/_exit host-side contract. Not dyld dispatch.
class LibSystemShim {
public:
    LibSystemShim(cpu::GuestMemory& memory, std::uint64_t heap_base,
                  std::size_t heap_capacity);

    LibSystemCall invoke(std::string_view symbol,
                         const std::array<std::uint64_t, 3>& guest_arguments,
                         std::uint64_t guest_thread_id = 1);

    const std::string& output() const { return calls_.standard_output(); }
    const std::string& errors() const { return calls_.standard_error(); }

private:
    cpu::GuestMemory& memory_;
    SyscallBridge calls_;
    std::uint64_t heap_base_;
    std::uint64_t next_;
    std::size_t capacity_;
    std::size_t committed_ = 0;
    std::map<std::uint64_t, std::uint64_t> thread_errno_;
    std::uint64_t ensure_errno(std::uint64_t guest_thread_id);
};
}
