#include <anyios/guest_environ.hpp>
#include <limits>
namespace anyios::darwin {
std::optional<std::string> GuestEnvironment::guest_name(std::uint64_t at) const {
    if (!at) return std::nullopt;
    std::string out;
    for (std::size_t i = 0; i <= kMaxNameBytes; ++i) {
        if (at > UINT64_MAX - i) return std::nullopt;
        const auto value = memory_.read(at + i, 1);
        if (!value) return std::nullopt;
        if (*value == 0) return out.empty() ? std::nullopt : std::optional<std::string>(out);
        if (*value < 33 || *value > 126 || *value == '=') return std::nullopt;
        out.push_back(static_cast<char>(*value));
    }
    return std::nullopt;
}
std::optional<std::uint64_t> GuestEnvironment::lookup(std::uint64_t name_at) const {
    const auto target = guest_name(name_at);
    if (!target || !envp_ || envp_ % 8 ||
        envp_ > UINT64_MAX - kMaxEntries * 8) return std::nullopt;
    for (std::size_t i = 0; i < kMaxEntries; ++i) {
        const auto address = memory_.read(envp_ + 8 * i, 8);
        if (!address) return std::nullopt;
        if (*address == 0) return std::uint64_t{0};
        if (*address > UINT64_MAX - kMaxEntryBytes) return std::nullopt;
        std::size_t equals = kMaxEntryBytes + 1;
        bool terminated = false;
        for (std::size_t n = 0; n <= kMaxEntryBytes; ++n) {
            const auto value = memory_.read(*address + n, 1);
            if (!value) return std::nullopt;
            if (*value == 0) { terminated = true; break; }
            if (*value == '=' && equals > kMaxEntryBytes) equals = n;
        }
        if (!terminated || equals == 0 || equals > kMaxEntryBytes) return std::nullopt;
        if (equals != target->size()) continue;
        bool match = true;
        for (std::size_t n = 0; n < equals; ++n) {
            if (memory_.read(*address + n, 1) !=
                static_cast<std::uint64_t>(static_cast<unsigned char>((*target)[n]))) {
                match = false;
                break;
            }
        }
        if (match) return *address + equals + 1; // original guest value bytes
    }
    return std::nullopt; // Unterminated/malformed envp vector
}
} // namespace anyios::darwin
