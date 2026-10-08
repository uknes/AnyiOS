#include <anyios/libsystem_shim.hpp>

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
    std::string_view symbol, const std::array<std::uint64_t, 3>& args,
    std::uint64_t guest_thread_id) {
    constexpr std::uint64_t max_linear_access = 64 * 1024;
    if (symbol == "_memcpy") {
        const auto destination = args[0];
        const auto source = args[1];
        const auto count = args[2];
        if (count > max_linear_access)
            throw std::invalid_argument("guest memcpy exceeds bounded byte limit");
        if (count == 0) return {destination, false};
        // memcpy overlap is undefined in C; explicitly refuse it rather than
        // silently emulating memmove for owned contract fixtures.
        if (destination <= source ? source - destination < count
                                  : destination - source < count) {
            throw std::invalid_argument("guest memcpy overlapping ranges are unsupported");
        }
        if (!memory_.allowed(source, static_cast<std::size_t>(count), cpu::Access::read) ||
            !memory_.allowed(destination, static_cast<std::size_t>(count), cpu::Access::write)) {
            throw std::invalid_argument("guest memcpy memory access denied");
        }
        std::vector<std::byte> data(static_cast<std::size_t>(count));
        if (!memory_.copy_from(source, data))
            throw std::runtime_error("guest memcpy source became unreadable");
        for (std::uint64_t i = 0; i < count; ++i) {
            if (!memory_.write(destination + i,
                               std::to_integer<std::uint8_t>(data[static_cast<std::size_t>(i)]), 1))
                throw std::runtime_error("guest memcpy destination changed permissions");
        }
        return {destination, false};
    }
    if (symbol == "_memset") {
        const auto destination = args[0];
        const auto fill = static_cast<std::uint8_t>(args[1]);
        const auto count = args[2];
        if (count > max_linear_access)
            throw std::invalid_argument("guest memset exceeds bounded byte limit");
        if (count == 0) return {destination, false};
        if (!memory_.allowed(destination, static_cast<std::size_t>(count), cpu::Access::write))
            throw std::invalid_argument("guest memset memory access denied");
        for (std::uint64_t i = 0; i < count; ++i) {
            if (!memory_.write(destination + i, fill, 1))
                throw std::runtime_error("guest memset destination changed permissions");
        }
        return {destination, false};
    }
    if (symbol == "_strlen" || symbol == "_strcmp") {
        auto scan = [&](std::uint64_t at, std::uint64_t limit) {
            if (limit > max_linear_access)
                throw std::invalid_argument("guest C string bounded scan exhausted");
            const auto v = memory_.read(at, 1);
            if (!v) throw std::invalid_argument("unreadable guest C string");
            return static_cast<std::uint8_t>(*v);
        };
        if (symbol == "_strlen") {
            for (std::uint64_t i = 0; i < max_linear_access; ++i) {
                if (args[0] > UINT64_MAX - i)
                    throw std::invalid_argument("guest strlen address overflow");
                if (scan(args[0] + i, i) == 0) return {i, false};
            }
            throw std::invalid_argument("guest strlen missing bounded terminator");
        }
        for (std::uint64_t i = 0; i < max_linear_access; ++i) {
            if (args[0] > UINT64_MAX - i || args[1] > UINT64_MAX - i)
                throw std::invalid_argument("guest strcmp address overflow");
            const auto left = scan(args[0] + i, i);
            const auto right = scan(args[1] + i, i);
            if (left != right) {
                // strcmp returns int. Only the low 32 bits of w0 are defined.
                const auto result = left < right ? -1 : 1;
                return {static_cast<std::uint32_t>(result), false};
            }
            if (left == 0) return {0, false};
        }
        throw std::invalid_argument("guest strcmp missing bounded terminator");
    }
    if (symbol == "___error") {
        return {ensure_errno(guest_thread_id), false};
    }
    if (symbol == "_abort") {
        throw GuestAbort{};
    }
    if (symbol == "_puts") {
        const auto length = invoke("_strlen", {args[0], 0, 0}, guest_thread_id).value;
        if (length >= 64 * 1024 || calls_.standard_output().size() >
                                      1024 * 1024 - static_cast<std::size_t>(length) - 1) {
            throw std::invalid_argument("guest puts output budget exceeded");
        }
        const auto scratch = invoke("_malloc", {16, 0, 0}, guest_thread_id).value;
        if (!scratch || !memory_.write(scratch, '\\n', 1)) {
            throw std::runtime_error("guest puts newline allocation failed");
        }
        const auto first = calls_.dispatch(0x80, 4, {1, args[0], length}, memory_);
        const auto second = calls_.dispatch(0x80, 4, {1, scratch, 1}, memory_);
        if (!first.supported || first.carry || !second.supported || second.carry)
            throw std::runtime_error("guest puts failed write contract");
        return {0, false}; // C puts promises only a nonnegative success value.
    }
    if (symbol == "_write") {
        const auto result = calls_.dispatch(0x80, 4, args, memory_);
        if (!result.supported)
            throw std::runtime_error("unsupported Darwin _write boundary");
        if (result.carry) {
            const auto errno_pointer = ensure_errno(guest_thread_id);
            if (!memory_.write(errno_pointer, result.value, 4))
                throw std::runtime_error("guest errno storage unwritable");
            return {UINT64_MAX, false}; // C ssize_t(-1), with Darwin errno set.
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
std::uint64_t LibSystemShim::ensure_errno(std::uint64_t thread_id) {
    if (thread_id == 0)
        throw std::invalid_argument("guest errno requires nonzero emulated thread id");
    if (const auto it = thread_errno_.find(thread_id); it != thread_errno_.end())
        return it->second;
    if (thread_errno_.size() >= 16)
        throw std::invalid_argument("bounded guest errno thread count exceeded");
    const auto pointer = invoke("_malloc", {16, 0, 0}, thread_id).value;
    if (!pointer || !memory_.write(pointer, 0, 4))
        throw std::runtime_error("could not initialize per-guest-thread errno");
    thread_errno_.emplace(thread_id, pointer);
    return pointer;
}
}
