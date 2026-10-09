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
    require(!memory.map_ios(0x28000, 4096, rx));
    require(!memory.map_ios(0x29000, 16384, rx));
    require(!memory.map_ios(0x28000, 16384, rx | anyios::cpu::bits(Access::write)));
    require(memory.map_ios(0x28000, 16384, rx));
    require(memory.allowed(0x2bffc, 4, Access::execute));
    require(!memory.write(0x28000, 0, 4));
    require(!memory.map_ios(0x28000, 16384, rx));

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

    {
        GuestMemory journal_memory(0x40000, 0x10000);
        require(journal_memory.map(0x40000, 4096, rw));
        require(journal_memory.write(0x40000, 0x12345678, 4));
        {
            GuestMemory::MappingJournal journal(journal_memory);
            require(journal.map(0x44000, 4096, rw));
            require(journal.load(0x44000, code));
            require(!journal.load(0x40000, code));
            require(!journal.map(0x44000, 4096, rw));
            require(journal_memory.read(0x44000, 8).has_value());
        }
        require(!journal_memory.read(0x44000, 8).has_value());
        require(journal_memory.read(0x40000, 4) == 0x12345678);
        {
            GuestMemory::MappingJournal journal(journal_memory);
            require(journal.map(0x44000, 4096, rw));
            require(journal.load(0x44000, code));
            journal.commit();
            require(!journal.map(0x45000, 4096, rw));
        }
        require(journal_memory.read(0x44000, 8).has_value());
        require(journal_memory.read(0x44000, 8).value() == 0xd65f03c0d2800540ULL);
        {
            GuestMemory::MappingJournal journal(journal_memory);
            require(!journal.map(0x45000, 4096, rw, true));
            require(journal.map(0x48000, 16384, rw, true));
        }
        require(!journal_memory.allowed(0x48000, 16384, Access::read));
        require(journal_memory.read(0x40000, 4) == 0x12345678);
    }
    // Bound must be checked *before* attempting a large host allocation.
    try {
        GuestMemory oversized(0x10000, GuestMemory::max_bytes + GuestMemory::page_size);
        static_cast<void>(oversized);
        throw std::runtime_error("oversized whole-app memory accepted");
    } catch (const std::invalid_argument&) {
    }
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
