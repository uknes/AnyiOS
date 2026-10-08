#include <anyios/guest_memory.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using anyios::cpu::Access;
using anyios::cpu::GuestMemory;
namespace {
void require(bool value) {
    if (!value) throw std::runtime_error("guest memory invariant violated");
}

void run() {
    GuestMemory memory(0x10000, 0x20000);
    const auto rx = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    require(memory.map(0x10000, 4096, rx));
    require(memory.map(0x20000, 8192, rw));
    require(!memory.map(0x10000, 4096, rx));
    require(!memory.map(0x1f000, 8192, rw));
    require(!memory.map(0x30000, 4096, rx | anyios::cpu::bits(Access::write)));
    require(!memory.map_ios(0x30000, 4096, rx));
    require(!memory.map_ios(0x31000, 16384, rx));
    require(!memory.map_ios(0x30000, 16384, rx | anyios::cpu::bits(Access::write)));
    require(memory.map_ios(0x30000, 16384, rx));
    require(memory.allowed(0x33ffc, 4, Access::execute));
    require(!memory.write(0x30000, 0, 4));
    require(!memory.map_ios(0x30000, 16384, rx));

    require(!memory.map(0x20001, 4096, rw));
    require(!memory.map(UINT64_MAX - 4095, 4096, rw));
    const std::array<std::byte, 8> code{
        std::byte{0x40}, std::byte{0x05}, std::byte{0x80}, std::byte{0xd2},
        std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
    };
    require(memory.load(0x10000, code));
    require(memory.fetch(0x10000) == 0xd2800540U);
    require(memory.fetch(0x10004) == 0xd65f03c0U);
    require(!memory.write(0x10000, 0, 4));
    require(!memory.read(0x10000, 16));
    require(!memory.fetch(0x20000));
    require(!memory.fetch(0x10001));
    require(!memory.read(0x1ffff, 8));
    require(memory.write(0x20ffc, 0x1122334455667788ULL, 8));
    require(memory.read(0x20ffc, 8) == 0x1122334455667788ULL);
    require(!memory.write(0x21fff, 0xaabb, 2));
    require(!memory.read(UINT64_MAX, 8));
    require(!memory.load(0x10000, {}));
    require(!memory.load(0x30000, code));
    try {
        GuestMemory bad(UINT64_MAX - 4095, 8192);
        static_cast<void>(bad);
        throw std::runtime_error("wrapped guest address space accepted");
    } catch (const std::invalid_argument&) {
    }
}
}
int main() {
    try {
        run();
        std::cout << "guest memory permission and range tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
