#include <anyios/objc_identity.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace anyios::darwin {
namespace {
std::uint64_t mapped_address(std::uint64_t address, std::uint64_t original,
                             std::uint64_t mapped) {
    if (address < original || address - original > UINT64_MAX - mapped) {
        throw std::invalid_argument("Objective-C section outside guest image");
    }
    return mapped + (address - original);
}
} // namespace

ObjcIdentityProbe::ObjcIdentityProbe(
    const macho::Image& image, const cpu::GuestMemory& memory,
    std::uint64_t mapped_base) : memory_(memory) {
    const auto text = std::find_if(image.segments.begin(), image.segments.end(),
        [](const macho::Segment& s) { return s.name == "__TEXT"; });
    if (text == image.segments.end()) {
        throw std::invalid_argument("Objective-C probe requires mapped __TEXT");
    }
    const auto original = text->vm_address;
    for (const auto& section : image.sections) {
        if (section.name != "__objc_classlist" &&
            section.name != "__objc_methname" &&
            section.name != "__objc_const" &&
            section.name != "__cstring") {
            continue;
        }
        if (!section.size || section.size > 65536) {
            throw std::invalid_argument("unsupported Objective-C metadata size");
        }
        const auto start = mapped_address(section.address, original, mapped_base);
        if (section.size > UINT64_MAX - start ||
            !memory.allowed(start, static_cast<std::size_t>(section.size),
                            cpu::Access::read)) {
            throw std::invalid_argument("Objective-C metadata not mapped readable");
        }
        if (section.name == "__objc_methname") {
            method_ranges_.emplace_back(start, start + section.size);
            continue;
        }
        if (section.name == "__cstring") {
            class_name_ranges_.emplace_back(start, start + section.size);
            continue;
        }
        if (section.name == "__objc_const") {
            constants_ranges_.emplace_back(start, start + section.size);
            continue;
        }
        if (section.size > 4096 || section.size % 8 != 0) {
            throw std::invalid_argument("unaligned Objective-C classlist");
        }
        for (std::uint64_t offset = 0; offset < section.size; offset += 8) {
            const auto cls = memory.read(start + offset, 8);
            if (!cls || !*cls || (*cls & 7) != 0 ||
                !memory.allowed(*cls, 40, cpu::Access::read)) {
                throw std::invalid_argument("unmapped ObjC classlist entry");
            }
            owned_classes_.insert(*cls);
        }
    }
}

std::optional<std::string> ObjcIdentityProbe::bounded_ascii(
    std::uint64_t address, const std::vector<Range>& ranges) const {
    for (const auto& [start, end] : ranges) {
        if (address < start || address >= end) continue;
        std::string name;
        for (auto at = address; at < end && name.size() < 128; ++at) {
            const auto ch = memory_.read(at, 1);
            if (!ch) return std::nullopt;
            if (*ch == 0) return name.empty() ? std::nullopt :
                std::optional<std::string>{name};
            if (*ch < 32 || *ch > 126) return std::nullopt;
            name.push_back(static_cast<char>(*ch));
        }
    }
    return std::nullopt;
}

std::optional<std::string> ObjcIdentityProbe::selector_name(
    std::uint64_t selector) const {
    return bounded_ascii(selector, method_ranges_);
}

bool ObjcIdentityProbe::within_constants(
    std::uint64_t address, std::uint64_t bytes) const {
    for (const auto& [begin, end] : constants_ranges_) {
        if (address >= begin && address <= end &&
            bytes <= end - address &&
            memory_.allowed(address, static_cast<std::size_t>(bytes),
                            cpu::Access::read)) return true;
    }
    return false;
}

std::optional<std::uint64_t> ObjcIdentityProbe::local_ro(
    std::uint64_t receiver) const {
    if (!is_local_class(receiver) || receiver > UINT64_MAX - 32) {
        return std::nullopt;
    }
    const auto value = memory_.read(receiver + 32, 8);
    if (!value) return std::nullopt;
    const auto ro = *value & ~std::uint64_t{7};
    if (!within_constants(ro, 40)) return std::nullopt;
    const auto instance_start = memory_.read(ro + 4, 4);
    const auto instance_size = memory_.read(ro + 8, 4);
    if (!instance_start || !instance_size ||
        *instance_size > 4096 || *instance_start > *instance_size) {
        return std::nullopt;
    }
    return ro;
}

std::optional<std::string> ObjcIdentityProbe::local_class_name(
    std::uint64_t receiver) const {
    const auto ro = local_ro(receiver);
    if (!ro) return std::nullopt;
    const auto name_ptr = memory_.read(*ro + 24, 8);
    if (!name_ptr) return std::nullopt;
    return bounded_ascii(*name_ptr, class_name_ranges_);
}

std::optional<std::uint32_t> ObjcIdentityProbe::local_instance_size(
    std::uint64_t receiver) const {
    const auto ro = local_ro(receiver);
    if (!ro) return std::nullopt;
    const auto size = memory_.read(*ro + 8, 4);
    if (!size || *size < 8 || *size > 4096) return std::nullopt;
    return static_cast<std::uint32_t>(*size);
}

std::optional<GuestObjcMethod> ObjcIdentityProbe::local_instance_method(
    std::uint64_t receiver, std::string_view method_name) const {
    const auto ro = local_ro(receiver);
    if (!ro || method_name.empty()) return std::nullopt;
    const auto methods = memory_.read(*ro + 32, 8);
    if (!methods || !within_constants(*methods, 8)) return std::nullopt;
    const auto entsize = memory_.read(*methods, 4);
    const auto count = memory_.read(*methods + 4, 4);
    // Clang's non-relative non-fragile method_list_t is 24-byte entries.
    // Relative methods and all other encodings remain unsupported.
    if (!entsize || !count || *entsize != 24 || *count > 64 ||
        !within_constants(*methods, 8 + (*count * 24))) return std::nullopt;
    for (std::uint64_t i = 0; i < *count; ++i) {
        const auto entry = *methods + 8 + (i * 24);
        const auto selector = memory_.read(entry, 8);
        const auto imp = memory_.read(entry + 16, 8);
        if (!selector || !imp) return std::nullopt;
        if (selector_name(*selector) != method_name) continue;
        if ((*imp & 3) != 0 || !memory_.fetch(*imp)) {
            return std::nullopt;
        }
        return GuestObjcMethod{*selector, *imp};
    }
    return std::nullopt;
}

std::vector<std::uint64_t> ObjcIdentityProbe::local_classes() const {
    return {owned_classes_.begin(), owned_classes_.end()};
}

bool ObjcIdentityProbe::is_local_class(std::uint64_t receiver) const {
    return owned_classes_.contains(receiver);
}

std::optional<std::uint64_t> ObjcIdentityProbe::invoke_class_identity(
    std::uint64_t receiver, std::uint64_t selector) const {
    // Precisely one supported message: +[locally declared class class].
    // It is safe only because class method +class returns the class object.
    // No arbitrary ObjC method dispatch, inheritance, or class initialization.
    if (!is_local_class(receiver) || selector_name(selector) != "class") {
        return std::nullopt;
    }
    return receiver;
}
} // namespace anyios::darwin
