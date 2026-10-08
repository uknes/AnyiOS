#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace anyios::darwin {

// Fail-closed introspection for compiler-owned ObjC metadata only.
// This does not implement general objc_msgSend or register Objective-C classes.
struct GuestObjcMethod {
    std::uint64_t selector = 0;
    std::uint64_t entry = 0;
};

class ObjcIdentityProbe {
public:
    ObjcIdentityProbe(const macho::Image& image, const cpu::GuestMemory& memory,
                      std::uint64_t mapped_base);

    std::optional<std::string> selector_name(std::uint64_t selector) const;
    bool is_local_class(std::uint64_t receiver) const;
    std::optional<std::string> local_class_name(std::uint64_t receiver) const;
    // Bounded, validated class_ro_t instance size for locally compiled classes.
    std::optional<std::uint32_t> local_instance_size(std::uint64_t receiver) const;
    std::optional<GuestObjcMethod> local_instance_method(
        std::uint64_t receiver, std::string_view name) const;
    std::optional<std::uint64_t> invoke_class_identity(
        std::uint64_t receiver, std::uint64_t selector) const;

private:
    const cpu::GuestMemory& memory_;
    std::set<std::uint64_t> owned_classes_;
    using Range = std::pair<std::uint64_t, std::uint64_t>;
    std::optional<std::string> bounded_ascii(
        std::uint64_t at, const std::vector<Range>& ranges) const;
    bool within_constants(std::uint64_t at, std::uint64_t bytes) const;
    std::optional<std::uint64_t> local_ro(std::uint64_t receiver) const;
    std::vector<Range> method_ranges_;
    std::vector<Range> class_name_ranges_;
    std::vector<Range> constants_ranges_;
};

}  // namespace anyios::darwin
