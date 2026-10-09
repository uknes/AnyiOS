#include <anyios/guest_os_log.hpp>
#include <array>
#include <stdexcept>
namespace anyios::darwin {
GuestOsLogRegistry::GuestOsLogRegistry(cpu::GuestMemory& memory, std::uint64_t guest_base)
    : memory_(memory), base_(guest_base) {
    if (!memory_.map_ios(base_, kArenaBytes, cpu::bits(cpu::Access::read))) {
        throw std::runtime_error("guest log token arena conflicts with guest mapping");
    }
}
std::optional<std::string> GuestOsLogRegistry::read_guest_string(std::uint64_t address) const {
    if (!address) return std::nullopt;
    std::string out;
    for (std::size_t i = 0; i <= kMaxNameBytes; ++i) {
        if (address > UINT64_MAX - i) return std::nullopt;
        const auto character = memory_.read(address + i, 1);
        if (!character) return std::nullopt;
        if (*character == 0) return out;
        if (*character < 0x20 || *character == 0x7f) return std::nullopt;
        out.push_back(static_cast<char>(*character));
    }
    return std::nullopt;
}
std::optional<std::uint64_t> GuestOsLogRegistry::create(std::uint64_t a, std::uint64_t b) {
    const auto subsystem = read_guest_string(a);
    const auto category = read_guest_string(b);
    if (!subsystem || !category || subsystem->empty()) return std::nullopt;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].subsystem == *subsystem && entries_[i].category == *category)
            return base_ + 16 * i;
    }
    if (entries_.size() >= kMaxObjects) return std::nullopt;
    constexpr std::array<std::byte, 16> opaque{};
    const auto token = base_ + 16 * entries_.size();
    if (!memory_.load(token, opaque)) return std::nullopt;
    entries_.push_back({*subsystem, *category});
    return token;
}
bool GuestOsLogRegistry::owns(std::uint64_t token) const noexcept {
    if (token < base_) return false;
    const auto offset = token - base_;
    return (offset % 16) == 0 && offset / 16 < entries_.size();
}
}
