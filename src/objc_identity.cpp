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
        if (section.name != "__objc_classlist" && section.name != "__objc_methname") {
            continue;
        }
        if (!section.size || section.size > 4096) {
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
        if (section.size % 8 != 0) {
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

std::optional<std::string> ObjcIdentityProbe::selector_name(
    std::uint64_t selector) const {
    for (const auto& [start, end] : method_ranges_) {
        if (selector < start || selector >= end) continue;
        std::string name;
        for (std::uint64_t at = selector; at < end && name.size() < 128; ++at) {
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
