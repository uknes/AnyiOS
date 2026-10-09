#include <anyios/guest_memory.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace anyios::cpu {
GuestMemory::GuestMemory(std::uint64_t base, std::size_t size)
    : base_(base) {
    if (size == 0 || size % page_size != 0 ||
        size > max_bytes ||
        base % page_size != 0 ||
        base > std::numeric_limits<std::uint64_t>::max() - size) {
        throw std::invalid_argument("invalid guest address-space configuration");
    }
    bytes_.resize(size);
    page_flags_.resize(size / page_size, 0);
}

std::optional<std::size_t> GuestMemory::offset_of(std::uint64_t address, std::size_t size) const {
    if (address < base_) return std::nullopt;
    const auto relative = address - base_;
    if (relative > bytes_.size() || size > bytes_.size() - relative) return std::nullopt;
    return static_cast<std::size_t>(relative);
}

bool GuestMemory::map(std::uint64_t address, std::size_t size, unsigned permissions) {
    if (size == 0 || (address % page_size) != 0 || (size % page_size) != 0 ||
        permissions == 0 || (permissions & ~7U) != 0 ||
        ((permissions & bits(Access::write)) && (permissions & bits(Access::execute)))) return false;
    const auto begin = offset_of(address, size);
    if (!begin) return false;
    const auto start_page = *begin / page_size;
    const auto page_count = size / page_size;
    for (std::size_t i = 0; i < page_count; ++i) {
        if (page_flags_[start_page + i] != 0) return false;
    }
    for (std::size_t i = 0; i < page_count; ++i) {
        page_flags_[start_page + i] = static_cast<unsigned char>(permissions);
    }
    return true;
}

bool GuestMemory::map_ios(std::uint64_t address, std::size_t size, unsigned permissions) {
    if (size == 0 || address % ios_page_size != 0 ||
        size % ios_page_size != 0) return false;
    return map(address, size, permissions);
}

void GuestMemory::unmap_owned(std::uint64_t address, std::size_t size) noexcept {
    const auto start = static_cast<std::size_t>(address - base_);
    const auto first = start / page_size;
    const auto count = size / page_size;
    std::fill(page_flags_.begin() + static_cast<std::ptrdiff_t>(first),
              page_flags_.begin() + static_cast<std::ptrdiff_t>(first + count), 0);
    std::fill(bytes_.begin() + static_cast<std::ptrdiff_t>(start),
              bytes_.begin() + static_cast<std::ptrdiff_t>(start + size), std::byte{0});
}

GuestMemory::MappingJournal::~MappingJournal() noexcept {
    if (committed_) return;
    for (auto it = owned_.rbegin(); it != owned_.rend(); ++it) {
        memory_.unmap_owned(it->address, it->size);
    }
}

bool GuestMemory::MappingJournal::map(std::uint64_t address, std::size_t size,
                                     unsigned permissions, bool require_ios_page) {
    if (committed_) return false;
    const bool mapped = require_ios_page ?
        memory_.map_ios(address, size, permissions) :
        memory_.map(address, size, permissions);
    if (!mapped) return false;
    try {
        owned_.push_back({address, size});
    } catch (...) {
        memory_.unmap_owned(address, size);
        throw;
    }
    return true;
}

bool GuestMemory::MappingJournal::load(std::uint64_t address,
                                      std::span<const std::byte> data) {
    if (committed_ || data.empty()) return false;
    for (const auto& region : owned_) {
        if (address >= region.address &&
            address - region.address <= region.size &&
            data.size() <= region.size - (address - region.address)) {
            return memory_.load(address, data);
        }
    }
    return false;
}

bool GuestMemory::allowed(std::uint64_t address, std::size_t size, Access access) const {
    if (size == 0) return false;
    const auto begin = offset_of(address, size);
    if (!begin) return false;
    const auto first_page = *begin / page_size;
    const auto last_page = (*begin + size - 1) / page_size;
    for (std::size_t i = first_page; i <= last_page; ++i) {
        if ((page_flags_[i] & bits(access)) == 0) return false;
    }
    return true;
}

bool GuestMemory::copy_from(std::uint64_t address, std::span<std::byte> destination) const {
    if (destination.empty() || !allowed(address, destination.size(), Access::read)) return false;
    const auto begin = *offset_of(address, destination.size());
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(begin), destination.size(), destination.begin());
    return true;
}

bool GuestMemory::load(std::uint64_t address, std::span<const std::byte> source) {
    const auto begin = offset_of(address, source.size());
    if (!begin || source.empty()) return false;
    const auto first = *begin / page_size;
    const auto last = (*begin + source.size() - 1) / page_size;
    for (std::size_t i = first; i <= last; ++i) {
        if (!page_flags_[i]) return false;
    }
    for (std::size_t i = 0; i < source.size(); ++i) {
        bytes_[*begin + i] = source[i];
    }
    return true;
}

std::optional<std::uint64_t> GuestMemory::read(std::uint64_t address, unsigned width) const {
    if (width != 1 && width != 2 && width != 4 && width != 8) return std::nullopt;
    if (!allowed(address, width, Access::read)) return std::nullopt;
    const auto begin = *offset_of(address, width);
    std::uint64_t result = 0;
    for (unsigned i = 0; i < width; ++i) {
        result |= std::uint64_t(std::to_integer<std::uint8_t>(bytes_[begin + i])) << (i * 8);
    }
    return result;
}

std::optional<std::uint32_t> GuestMemory::fetch(std::uint64_t address) const {
    if ((address & 3) != 0 || !allowed(address, 4, Access::execute)) return std::nullopt;
    const auto begin = *offset_of(address, 4);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i) {
        result |= std::uint32_t(std::to_integer<std::uint8_t>(bytes_[begin + i])) << (i * 8);
    }
    return result;
}

bool GuestMemory::write(std::uint64_t address, std::uint64_t value, unsigned width) {
    if (width != 1 && width != 2 && width != 4 && width != 8) return false;
    if (!allowed(address, width, Access::write)) return false;
    const auto begin = *offset_of(address, width);
    for (unsigned i = 0; i < width; ++i) {
        bytes_[begin + i] = std::byte((value >> (i * 8)) & 0xff);
    }
    return true;
}
}
