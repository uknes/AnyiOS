#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace anyios::darwin {

// Fail-closed introspection for compiler-owned ObjC metadata only.
// This does not implement general objc_msgSend or register Objective-C classes.
class ObjcIdentityProbe {
public:
    ObjcIdentityProbe(const macho::Image& image, const cpu::GuestMemory& memory,
                      std::uint64_t mapped_base);

    std::optional<std::string> selector_name(std::uint64_t selector) const;
    bool is_local_class(std::uint64_t receiver) const;
    std::optional<std::uint64_t> invoke_class_identity(
        std::uint64_t receiver, std::uint64_t selector) const;

private:
    const cpu::GuestMemory& memory_;
    std::set<std::uint64_t> owned_classes_;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> method_ranges_;
};

}  // namespace anyios::darwin
