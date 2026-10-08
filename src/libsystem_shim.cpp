#include <anyios/libsystem_shim.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <limits>
#include <stdexcept>
#include <string>

namespace anyios::darwin {
LibSystemShim::LibSystemShim(cpu::GuestMemory& memory, std::uint64_t heap_base,
                             std::size_t heap_capacity)
    : memory_(memory), heap_base_(heap_base), next_(heap_base),
      capacity_(heap_capacity) {
    if (heap_base % cpu::GuestMemory::ios_page_size != 0 ||
        heap_capacity == 0 || heap_capacity > 1024 * 1024 ||
        heap_capacity % cpu::GuestMemory::ios_page_size != 0 ||
        heap_base > std::numeric_limits<std::uint64_t>::max() - heap_capacity) {
        throw std::invalid_argument("invalid bounded Darwin guest heap");
    }
}

LibSystemCall LibSystemShim::invoke(
    std::string_view symbol, const std::array<std::uint64_t, 3>& args) {
    if (symbol == "_write") {
        const auto result = calls_.dispatch(0x80, 4, args, memory_);
        if (!result.supported || result.carry) {
            throw std::runtime_error("unsupported Darwin _write or guest errno boundary");
        }
        return {result.value, false};
    }
    if (symbol == "_exit") {
        const auto result = calls_.dispatch(0x80, 1, args, memory_);
        if (!result.supported || !result.exited) {
            throw std::runtime_error("unsupported Darwin _exit");
        }
        return {result.value, true};
    }
    if (symbol == "_malloc") {
        const auto requested = args[0] == 0 ? std::uint64_t{16} : args[0];
        if (requested > capacity_ || requested > UINT64_MAX - 15) {
            return {0, false};
        }
        const auto bytes = (requested + 15) & ~std::uint64_t{15};
        const auto used = next_ - heap_base_;
        if (bytes > capacity_ - used) return {0, false};
        const auto result = next_;
        const auto required = used + bytes;
        const auto page = cpu::GuestMemory::ios_page_size;
        const auto needed = ((required + page - 1) / page) * page;
        cpu::GuestMemory::MappingJournal journal(memory_);
        for (auto offset = committed_; offset < needed; offset += page) {
            if (!journal.map(heap_base_ + offset, page,
                             cpu::bits(cpu::Access::read) |
                             cpu::bits(cpu::Access::write), true)) {
                return {0, false};
            }
        }
        journal.commit();
        committed_ = static_cast<std::size_t>(needed);
        next_ += bytes;
        return {result, false};
    }
    // Owned ABI slice: x0..x2 carry pointers, size_t, and the low 32 bits
    // of int. Guest addresses are validated against GuestMemory, never cast to
    // host pointers. Large/unmapped and unterminated inputs fail explicitly.
    constexpr std::uint64_t max_buffer = 64 * 1024;
    if (symbol == "_memcpy" || symbol == "_memset") {
        const auto destination = args[0];
        const auto length = args[2];
        if (length == 0) return {destination, false};
        if (length > max_buffer ||
            !memory_.allowed(destination, static_cast<std::size_t>(length),
                             cpu::Access::write)) {
            throw std::runtime_error("unsupported libSystem guest buffer length/destination");
        }
        std::vector<std::byte> bytes(static_cast<std::size_t>(length));
        if (symbol == "_memset") {
            const auto value = static_cast<std::uint8_t>(args[1] & 0xffU);
            std::fill(bytes.begin(), bytes.end(), std::byte{value});
        } else {
            const auto source = args[1];
            // memcpy overlapping ranges are undefined; reject rather than
            // reinterpreting them as memmove.
            if (source <= destination ?
                destination - source < length : source - destination < length) {
                throw std::runtime_error("overlapping guest memcpy is unsupported");
            }
            if (!memory_.copy_from(source, bytes)) {
                throw std::runtime_error("unreadable guest memcpy source");
            }
        }
        if (!memory_.copy_to(destination, bytes)) {
            throw std::runtime_error("guest buffer write denied");
        }
        return {destination, false};
    }
    if (symbol == "_strlen" || symbol == "_strcmp") {
        for (std::uint64_t i = 0; i < max_buffer; ++i) {
            if (args[0] > UINT64_MAX - i ||
                (symbol == "_strcmp" && args[1] > UINT64_MAX - i)) {
                throw std::runtime_error("guest string address overflow");
            }
            const auto first = memory_.read(args[0] + i, 1);
            if (!first) throw std::runtime_error("unreadable guest C string");
            if (symbol == "_strlen") {
                if (*first == 0) return {i, false};
            } else {
                const auto second = memory_.read(args[1] + i, 1);
                if (!second) throw std::runtime_error("unreadable guest strcmp argument");
                // strcmp returns only a negative/zero/positive int; the
                // Apple ARM64 caller observes the low 32-bit result in w0.
                if (*first != *second) {
                    const auto sign = *first < *second ? -1 : 1;
                    return {static_cast<std::uint32_t>(sign), false};
                }
                if (*first == 0) return {0, false};
            }
        }
        throw std::runtime_error("unterminated guest C string exceeds runtime bound");
    }
    throw std::invalid_argument("unimplemented libSystem symbol: " + std::string(symbol));
}
}
