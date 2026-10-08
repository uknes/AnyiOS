#pragma once

#include <anyios/darwin_syscall.hpp>
#include <anyios/guest_memory.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace anyios::darwin {

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
                         const std::array<std::uint64_t, 3>& guest_arguments);

    const std::string& output() const { return calls_.standard_output(); }
    const std::string& errors() const { return calls_.standard_error(); }

private:
    cpu::GuestMemory& memory_;
    SyscallBridge calls_;
    std::uint64_t heap_base_;
    std::uint64_t next_;
    std::size_t capacity_;
    std::size_t committed_ = 0;
};
}
