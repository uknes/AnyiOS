#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace anyios::cpu {

enum class Access : std::uint8_t {
    read = 1,
    write = 2,
    execute = 4,
};

constexpr unsigned bits(Access access) {
    return static_cast<unsigned>(access);
}

class GuestMemory {
public:
    static constexpr std::size_t page_size = 4096;
    static constexpr std::size_t ios_page_size = 16 * 1024;

    GuestMemory(std::uint64_t base, std::size_t size);

    bool map(std::uint64_t address, std::size_t size, unsigned permissions);
    bool map_ios(std::uint64_t address, std::size_t size, unsigned permissions);
    bool load(std::uint64_t address, std::span<const std::byte> source);
    std::optional<std::uint64_t> read(std::uint64_t address, unsigned width) const;
    std::optional<std::uint32_t> fetch(std::uint64_t address) const;
    bool write(std::uint64_t address, std::uint64_t value, unsigned width);
    bool allowed(std::uint64_t address, std::size_t size, Access access) const;
    bool copy_from(std::uint64_t address, std::span<std::byte> destination) const;

private:
    std::optional<std::size_t> offset_of(std::uint64_t address, std::size_t size) const;

    std::uint64_t base_;
    std::vector<std::byte> bytes_;
    std::vector<unsigned char> page_flags_;
};

}
