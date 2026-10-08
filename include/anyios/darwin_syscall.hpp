#pragma once

#include <anyios/guest_memory.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace anyios::darwin {

struct TrapResult {
    std::uint64_t value = 0;
    bool carry = false;
    bool supported = true;
    bool exited = false;
    std::string diagnostic;
};

class SyscallBridge {
public:
    TrapResult dispatch(std::uint32_t svc,
                        std::uint64_t number,
                        const std::array<std::uint64_t, 3>& arguments,
                        const cpu::GuestMemory& memory);

    const std::string& standard_output() const { return stdout_; }
    const std::string& standard_error() const { return stderr_; }

private:
    std::string stdout_;
    std::string stderr_;
};

}
