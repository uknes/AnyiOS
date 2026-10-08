#include <anyios/native_a64.hpp>
#include <anyios/cpu_backend.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/object_code.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool good, const char* reason) {
    if (!good) throw std::runtime_error(reason);
}
void rejects(std::span<const std::byte> code) {
    try {
        (void)anyios::cpu::execute_owned_arm64_fixture(code);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid native ARM64 code was accepted");
}
}

int main(int argc, char** argv) {
    try {
        constexpr std::array<std::byte, 8> known{
            std::byte{0x40}, std::byte{0x05}, std::byte{0x80}, std::byte{0xd2},
            std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
        };
        check(anyios::cpu::execute_owned_arm64_fixture(known) == 42, "native ARM64 return value");
        {
            anyios::cpu::GuestMemory memory(0x10000, 0x4000);
            const auto rx = anyios::cpu::bits(anyios::cpu::Access::read) |
                            anyios::cpu::bits(anyios::cpu::Access::execute);
            check(memory.map(0x10000, 0x1000, rx), "native backend guest code map");
            check(memory.load(0x10000, known), "native backend guest code load");
            auto backend = anyios::cpu::make_native_fixture_backend(memory);
            anyios::cpu::CpuState initial;
            initial.pc = 0x10000;
            initial.x[30] = 0x20000;
            backend->set_state(initial);
            const auto event = backend->step();
            const auto state = backend->state();
            check(event.kind == anyios::cpu::CpuEventKind::stepped &&
                  state.x[0] == 42 && state.pc == 0x20000,
                  "native and Dynarmic CPU contracts diverged");
            initial.pc = 0x11000;
            backend->set_state(initial);
            check(backend->step().kind == anyios::cpu::CpuEventKind::fault,
                  "unmapped guest fetch accepted");
            constexpr std::array<std::byte, 4> darwin_svc{
                std::byte{0x01}, std::byte{0x10}, std::byte{0x00}, std::byte{0xd4}
            };
            check(memory.map(0x12000, 4096, rx), "SVC sentinel mapping");
            check(memory.load(0x12000, darwin_svc), "SVC sentinel load");
            initial.pc = 0x12000;
            backend->set_state(initial);
            const auto trap = backend->step();
            check(trap.kind == anyios::cpu::CpuEventKind::unsupported,
                  "native Darwin SVC reached the host kernel");
            check(backend->state().pc == 0x12000,
                  "native SVC failure mutated guest program counter");
        }
        auto invalid = known;
        invalid[4] = std::byte{0};
        rejects(invalid);
        rejects(std::span<const std::byte>(known).first(4));
        if (argc == 2) {
            std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
            if (!input) throw std::runtime_error("owned Mach-O object not found");
            const auto length = input.tellg();
            if (length <= 0 || length > 4 * 1024 * 1024) {
                throw std::runtime_error("owned Mach-O object size invalid");
            }
            input.seekg(0);
            std::vector<std::byte> file(static_cast<std::size_t>(length));
            if (!input.read(reinterpret_cast<char*>(file.data()),
                            static_cast<std::streamsize>(file.size()))) {
                throw std::runtime_error("owned Mach-O object read failed");
            }
            const auto function = anyios::loader::extract_object_function(file, "_anyios_answer");
            check(anyios::cpu::execute_owned_arm64_fixture(function.code) == 42,
                  "compiled Mach-O ARM64 code returned wrong value");
            std::cout << "Native ARM64 executed project-owned iOS Mach-O function: 42\n";
        } else if (argc == 1) {
            std::cout << "Native ARM64 executed verified owned code: 42\n";
        } else {
            throw std::runtime_error("usage: anyios-native-a64-smoke [owned-arm64-object]");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
