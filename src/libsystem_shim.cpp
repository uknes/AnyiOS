#include <anyios/libsystem_shim.hpp>

#include <cstdint>
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
    throw std::invalid_argument("unimplemented libSystem symbol: " + std::string(symbol));
}
}
