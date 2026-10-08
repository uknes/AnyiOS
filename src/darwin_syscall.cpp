#include <anyios/darwin_syscall.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace anyios::darwin {
namespace {
constexpr std::uint64_t sys_exit = 1;
constexpr std::uint64_t sys_write = 4;
constexpr std::size_t max_write = 64 * 1024;
constexpr std::size_t max_output = 1024 * 1024;

TrapResult error(std::uint64_t err) {
    return {err, true, true, false, {}};
}
}

TrapResult SyscallBridge::dispatch(std::uint32_t svc,
                                   std::uint64_t number,
                                   const std::array<std::uint64_t, 3>& arguments,
                                   const cpu::GuestMemory& memory) {
    if (svc != 0x80) {
        return {0, false, false, false, "unsupported ARM64 SVC immediate"};
    }
    if (number == sys_exit) {
        return {arguments[0] & 0xff, false, true, true, {}};
    }
    if (number != sys_write) {
        return {0, false, false, false, "unsupported Darwin syscall " + std::to_string(number)};
    }
    const auto fd = arguments[0];
    if (fd != 1 && fd != 2) return error(9);
    const auto length = arguments[2];
    if (length > max_write) return error(22);
    auto& destination = fd == 1 ? stdout_ : stderr_;
    if (length > max_output - destination.size()) return error(22);
    if (length == 0) return {0, false, true, false, {}};
    std::vector<std::byte> data(static_cast<std::size_t>(length));
    if (!memory.copy_from(arguments[1], data)) return error(14);
    destination.append(reinterpret_cast<const char*>(data.data()), data.size());
    return {length, false, true, false, {}};
}
}
