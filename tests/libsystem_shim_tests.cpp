#include <anyios/libsystem_shim.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool yes, std::string_view what) {
    if (!yes) throw std::runtime_error(std::string(what));
}
}
int main() {
    try {
        anyios::cpu::GuestMemory memory(0x10000, 0x80000);
        anyios::darwin::LibSystemShim lib(memory, 0x20000, 0x10000);
        const auto first = lib.invoke("_malloc", {8, 0, 0});
        const auto second = lib.invoke("_malloc", {17, 0, 0});
        require(first.value == 0x20000 && second.value == 0x20010,
                "bump allocator alignment");
        require(memory.allowed(first.value, 16, anyios::cpu::Access::write),
                "returned guest pointer must be writable");
        require(memory.write(first.value, 'O', 1) &&
                memory.write(first.value + 1, 'K', 1), "prepare owned buffer");
        const auto written = lib.invoke("_write", {1, first.value, 2});
        require(written.value == 2 && lib.output() == "OK", "captured guest stdout");
        require(lib.invoke("_exit", {23, 0, 0}).exited, "guest exit not signaled");
        require(lib.invoke("_malloc", {1024 * 1024, 0, 0}).value == 0,
                "heap overcommit must return null guest pointer");
        require(lib.invoke("_malloc", {16, 0, 0}).value != 0,
                "heap failed after oversized allocation");
        require(!memory.fetch(first.value), "heap memory became executable");
        const auto rw = anyios::cpu::bits(anyios::cpu::Access::read) |
                        anyios::cpu::bits(anyios::cpu::Access::write);
        require(memory.map_ios(0x40000, 0x4000, rw), "C primitives test data page");
        const std::array<std::byte, 8> c_string{
            std::byte{'A'}, std::byte{'B'}, std::byte{0},
            std::byte{'C'}, std::byte{'D'}, std::byte{0},
            std::byte{0xff}, std::byte{0}
        };
        require(memory.load(0x40000, c_string), "C primitive string bytes");
        require(lib.invoke("_strlen", {0x40000, 0, 0}).value == 2,
                "strlen bounded termination");
        require(lib.invoke("_strlen", {0x40006, 0, 0}).value == 1,
                "strlen unsigned-byte high bit");
        require(lib.invoke("_strcmp", {0x40000, 0x40000, 0}).value == 0,
                "strcmp exact equality");
        require(lib.invoke("_strcmp", {0x40000, 0x40003, 0}).value == 0xffffffffULL,
                "strcmp negative signed int must use low 32 bits");
        require(lib.invoke("_strcmp", {0x40003, 0x40000, 0}).value == 1,
                "strcmp positive unsigned-byte ordering");
        require(lib.invoke("_memcpy", {0x40080, 0x40000, 3}).value == 0x40080,
                "memcpy must return original guest destination pointer");
        require(memory.read(0x40080, 1) == static_cast<unsigned>('A') &&
                memory.read(0x40081, 1) == static_cast<unsigned>('B') &&
                memory.read(0x40082, 1) == 0, "memcpy copied bytes incorrectly");
        require(lib.invoke("_memset", {0x40080, 0x1234ff, 2}).value == 0x40080 &&
                memory.read(0x40080, 1) == 255 && memory.read(0x40081, 1) == 255 &&
                memory.read(0x40082, 1) == 0,
                "memset must truncate c to unsigned char without clobbering tail");
        require(lib.invoke("_memcpy", {0, 0, 0}).value == 0,
                "zero-length memcpy should not dereference pointers");
        require(lib.invoke("_memset", {0, 0x1234, 0}).value == 0,
                "zero-length memset should not dereference pointers");
        auto refuse = [&](std::string_view symbol, const std::array<std::uint64_t, 3>& args) {
            try {
                static_cast<void>(lib.invoke(symbol, args));
                throw std::runtime_error("unsupported guest libc access returned success");
            } catch (const std::invalid_argument&) {}
        };
        refuse("_memcpy", {0x40001, 0x40000, 4});
        refuse("_memcpy", {0x50000, 0x40000, 4});
        refuse("_memset", {0x50000, 0, 4});
        refuse("_memcpy", {0x40080, 0x40000, 65537});
        refuse("_strlen", {0x50000, 0, 0});
        refuse("_strcmp", {0x40000, 0x50000, 0});
        require(memory.read(0x40082, 1) == 0,
                "invalid libc call corrupted previously copied destination");
        try {
            (void)lib.invoke("_unknown", {0, 0, 0});
            throw std::runtime_error("missing libSystem symbol was accepted");
        } catch (const std::invalid_argument&) { }
        const auto errno_one = lib.invoke("___error", {0, 0, 0}, 11).value;
        const auto errno_two = lib.invoke("___error", {0, 0, 0}, 12).value;
        require(errno_one && errno_two && errno_one != errno_two &&
                memory.read(errno_one, 4) == 0 && memory.read(errno_two, 4) == 0,
                "thread-scoped errno guest storage must be distinct and zeroed");
        require(lib.invoke("_write", {99, first.value, 2}, 11).value == UINT64_MAX &&
                memory.read(errno_one, 4) == 9 && memory.read(errno_two, 4) == 0,
                "invalid descriptor must return ssize_t(-1) and set only caller errno");
        require(lib.invoke("___error", {0, 0, 0}, 11).value == errno_one,
                "errno address changed during thread lifetime");
        require(lib.output() == "OK", "failed write changed guest output");
        require(lib.invoke("_puts", {0x40000, 0, 0}).value == 0 &&
                lib.output() == "OKAB\n", "puts must append line through bounded guest write");
        try {
            static_cast<void>(lib.invoke("_abort", {0, 0, 0}));
            throw std::runtime_error("guest abort returned successfully");
        } catch (const anyios::darwin::GuestAbort&) {}
        require(lib.invoke("___error", {0, 0, 0}).value != 0,
                "default guest thread errno address missing");
        std::cout << "Minimal host libSystem _malloc/_write/_exit contract passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
