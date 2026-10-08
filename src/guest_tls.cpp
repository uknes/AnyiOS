#include <anyios/guest_tls.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace anyios::darwin {
namespace {
constexpr auto page = cpu::GuestMemory::ios_page_size;
constexpr auto rw = cpu::bits(cpu::Access::read) | cpu::bits(cpu::Access::write);

std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > UINT64_MAX - a) throw macho::FormatError("TLV guest address overflow");
    return a + b;
}
}

GuestTls::GuestTls(cpu::GuestMemory& memory, std::uint64_t base,
                   std::size_t capacity)
    : memory_(memory), next_(base) {
    if (base % page || capacity < page || capacity > 64 * page ||
        capacity % page || base > UINT64_MAX - capacity) {
        throw std::invalid_argument("invalid bounded guest TLS arena");
    }
    end_ = base + capacity;
}

std::uint64_t GuestTls::new_page(std::span<const std::byte> initial) {
    if (initial.size() > page || next_ >= end_ || end_ - next_ < page)
        throw macho::FormatError("guest TLS page budget exceeded");
    cpu::GuestMemory::MappingJournal journal(memory_);
    const auto address = next_;
    if (!journal.map(address, page, rw, true) ||
        (!initial.empty() && !journal.load(address, initial))) {
        throw macho::FormatError("guest TLS mapping/initialization failed");
    }
    journal.commit();
    next_ += page;
    return address;
}

void GuestTls::register_module(const macho::Image& image,
                               std::uint64_t guest_base) {
    if (modules_.size() >= 16) throw macho::FormatError("too many guest TLV modules");
    const auto text = std::find_if(image.segments.begin(), image.segments.end(),
        [](const macho::Segment& item) { return item.name == "__TEXT"; });
    if (text == image.segments.end())
        throw macho::FormatError("TLV module lacks __TEXT base");
    // The linked descriptor's third word is a byte offset into the module's
    // contiguous TLV initialization image, not an ASLR-slid guest pointer.
    // Build that image in guest virtual-address order, preserving bounded gaps.
    std::vector<const macho::Section*> templates;
    for (const auto& section : image.sections) {
        if (section.name == "__thread_data" || section.name == "__thread_bss")
            templates.push_back(&section);
    }
    if (templates.empty() || templates.size() > 32)
        throw macho::FormatError("missing or excessive TLV template sections");
    std::sort(templates.begin(), templates.end(),
        [](const macho::Section* a, const macho::Section* b) {
            return a->address < b->address;
        });
    const auto origin = templates.front()->address;
    Module module;
    std::uint64_t end = origin;
    for (const auto* section : templates) {
        const auto kind = section->flags & 0xffu;
        if ((section->name == "__thread_data" &&
             (kind != 0x11u || section->zero_fill)) ||
            (section->name == "__thread_bss" &&
             (kind != 0x12u || !section->zero_fill)) ||
            section->address < text->vm_address || section->size == 0 ||
            section->size > page || section->address < end ||
            section->segment_name.rfind("__DATA", 0) != 0) {
            throw macho::FormatError("unsupported TLV initialization section");
        }
        end = checked_add(section->address, section->size);
        if (end - origin > page)
            throw macho::FormatError("TLV template exceeds guest page");
        module.initial.resize(static_cast<std::size_t>(end - origin));
        const auto guest = checked_add(guest_base, section->address - text->vm_address);
        std::vector<std::byte> bytes(static_cast<std::size_t>(section->size));
        if (!memory_.copy_from(guest, bytes))
            throw macho::FormatError("unreadable TLV init section");
        std::copy(bytes.begin(), bytes.end(),
                  module.initial.begin() +
                  static_cast<std::ptrdiff_t>(section->address - origin));
    }
    for (const auto& section : image.sections) {
        if (section.name != "__thread_vars") continue;
        if ((section.flags & 0xffu) != 0x13u ||
            section.segment_name.rfind("__DATA", 0) != 0 ||
            section.address < text->vm_address || section.size == 0 ||
            section.size > 24 * 64 || section.size % 24 != 0 ||
            regions.empty()) {
            throw macho::FormatError("unsupported __thread_vars metadata");
        }
        const auto begin = checked_add(guest_base, section.address - text->vm_address);
        for (std::uint64_t off = 0; off < section.size; off += 24) {
            const auto descriptor = checked_add(begin, off);
            const auto resolver = memory_.read(descriptor, 8);
            const auto reserved = memory_.read(descriptor + 8, 8);
            const auto original = memory_.read(descriptor + 16, 8);
            if (!resolver || !reserved || !original ||
                *reserved != 0 || !memory_.fetch(*resolver)) {
                throw macho::FormatError("invalid/unbound TLV descriptor");
            }
            if (*original >= module.initial.size())
                throw macho::FormatError("TLV descriptor template offset out of range");
            module.variables.push_back({descriptor, *original});
        }
    }
    if (module.variables.empty() || module.variables.size() > 64 ||
        module.initial.empty()) {
        throw macho::FormatError("no supported TLV descriptors and templates");
    }
    modules_.push_back(std::move(module));
}

std::uint64_t GuestTls::thread_register(std::uint64_t thread_id) {
    if (thread_id == 0) throw macho::FormatError("zero guest thread id");
    if (const auto it = headers_.find(thread_id); it != headers_.end())
        return it->second;
    std::array<std::byte, 8> contents{};
    for (unsigned i = 0; i < contents.size(); ++i) {
        contents[i] = std::byte((thread_id >> (8 * i)) & 255);
    }
    const auto address = new_page(contents);
    headers_.emplace(thread_id, address);
    return address;
}

std::uint64_t GuestTls::get_address(std::uint64_t thread_id,
                                    std::uint64_t descriptor) {
    if (thread_id == 0 || !headers_.contains(thread_id)) {
        throw macho::FormatError("TLV lookup without active guest thread");
    }
    for (std::size_t i = 0; i < modules_.size(); ++i) {
        const auto& mod = modules_[i];
        const auto it = std::find_if(mod.variables.begin(), mod.variables.end(),
            [&](const Variable& v) { return v.descriptor == descriptor; });
        if (it == mod.variables.end()) continue;
        const auto key = std::make_pair(thread_id, i);
        auto found = thread_modules_.find(key);
        if (found == thread_modules_.end()) {
            const auto base = new_page(mod.initial);
            found = thread_modules_.emplace(key, base).first;
        }
        return checked_add(found->second, it->module_offset);
    }
    throw macho::FormatError("unregistered guest TLV descriptor");
}

void GuestTls::finish_thread(std::uint64_t thread_id) {
    if (thread_id == 0 || !headers_.contains(thread_id))
        throw macho::FormatError("unknown guest thread termination");
    for (const auto& [key, address] : thread_modules_) {
        static_cast<void>(address);
        if (key.first == thread_id) {
            throw macho::FormatError(
                "guest TLV teardown/destructors not supported; refusing thread exit");
        }
    }
    // Live header is retained; guest thread identifiers cannot be reused yet.
    throw macho::FormatError("guest TLS thread teardown not implemented");
}
} // namespace anyios::darwin
