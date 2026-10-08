#include <anyios/libsystem_shim.hpp>

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
        try {
            (void)lib.invoke("_unknown", {0, 0, 0});
            throw std::runtime_error("missing libSystem symbol was accepted");
        } catch (const std::invalid_argument&) { }
        try {
            (void)lib.invoke("_write", {99, first.value, 2});
            throw std::runtime_error("invalid host fd silently accepted");
        } catch (const std::runtime_error& e) {
            if (std::string_view(e.what()).find("unsupported Darwin") == std::string_view::npos)
                throw;
        }
        require(lib.output() == "OK", "failed write changed guest output");
        std::cout << "Minimal host libSystem _malloc/_write/_exit contract passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
