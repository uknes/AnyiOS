#include <anyios/guest_memory.hpp>
#include <anyios/cpu_backend.hpp>
#include <anyios/object_code.hpp>
#include <anyios/executable.hpp>
#include <anyios/linked_pair.hpp>
#include <anyios/darwin_syscall.hpp>


#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;

void execute_guest(GuestMemory& memory, std::uint64_t entry, std::uint64_t return_address,
                   anyios::darwin::SyscallBridge* bridge = nullptr,
                   std::uint64_t syscall_buffer = 0,
                   std::uint64_t syscall_length = 0) {
    auto backend = anyios::cpu::make_dynarmic_backend(memory);
    anyios::cpu::CpuState guest;
    guest.pc = entry;
    guest.x[30] = return_address;
    if (bridge) {
        guest.x[1] = syscall_buffer;
        guest.x[2] = syscall_length;
    }
    backend->set_state(guest);
    bool returned = false;
    for (unsigned i = 0; i < 128; ++i) {
        const auto event = backend->step();
        if (event.kind == anyios::cpu::CpuEventKind::fault ||
            event.kind == anyios::cpu::CpuEventKind::unsupported) {
            throw std::runtime_error(event.diagnostic);
        }
        guest = backend->state();
        if (event.kind == anyios::cpu::CpuEventKind::svc) {
            if (!bridge) throw std::runtime_error("guest SVC requires a Darwin bridge");
            const auto result = bridge->dispatch(event.svc_immediate, guest.x[16],
                {guest.x[0], guest.x[1], guest.x[2]}, memory);
            if (!result.supported) throw std::runtime_error(result.diagnostic);
            if (result.exited) throw std::runtime_error("unexpected guest exit syscall");
            guest.x[0] = result.value;
            guest.pstate = (guest.pstate & ~(std::uint32_t(1) << 29)) |
                           (std::uint32_t(result.carry) << 29);
            backend->set_state(guest);
        }
        if (guest.pc == return_address) {
            returned = true;
            break;
        }
    }
    if (!returned || guest.x[0] != 42) {
        throw std::runtime_error("ARM64 code returned an incorrect result");
    }
}


std::vector<std::byte> read_owned_binary(const char* path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error(std::string("missing owned iPhoneOS image: ") + path);
    const auto length = stream.tellg();
    if (length <= 0 || length > 32 * 1024 * 1024) {
        throw std::runtime_error("owned linked Mach-O image exceeds size limit");
    }
    stream.seekg(0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("could not read owned linked Mach-O image");
    }
    return bytes;
}

void verify_real_linked_call(const std::vector<std::byte>& main_image,
                             const std::vector<std::byte>& library_image) {
    GuestMemory memory(0x10000, 4 * 1024 * 1024);
    const auto loaded = anyios::loader::stage_owned_linked_pair(
        main_image, library_image, memory, 0x10000, 0x80000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    constexpr std::uint64_t stack_address = 0x300000;
    constexpr std::uint64_t stack_bytes = 0x10000;
    constexpr std::uint64_t return_sentinel = 0x400000;
    if (!memory.map(stack_address, stack_bytes, rw)) {
        throw std::runtime_error("guest stack mapping failed");
    }
    if (!memory.fetch(loaded.imported_function) || !memory.fetch(loaded.entry)) {
        throw std::runtime_error("linked guest function is not executable");
    }
    auto backend = anyios::cpu::make_dynarmic_backend(memory);
    anyios::cpu::CpuState guest;
    guest.pc = loaded.entry;
    guest.sp = stack_address + stack_bytes;
    guest.x[30] = return_sentinel;
    backend->set_state(guest);
    const auto event = backend->run_until_event(4096, return_sentinel);
    const auto state = backend->state();
    if (event.kind != anyios::cpu::CpuEventKind::returned) {
        throw std::runtime_error("linked ARM64 guest did not return: " +
                                 event.diagnostic + " at PC " + std::to_string(state.pc));
    }
    if (state.x[0] != 42) {
        throw std::runtime_error("cross-dylib ARM64 guest call returned incorrect value");
    }
    std::cout << "Executed real iPhoneOS MH_EXECUTE -> MH_DYLIB call on Windows x64: 42\n";
}

void verify_syscall_object(const std::vector<std::byte>& code) {
    GuestMemory memory(0x10000, 0x30000);
    const auto rx = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    if (!memory.map(0x10000, 4096, rx) || !memory.map(0x20000, 4096, rw)) {
        throw std::runtime_error("syscall test guest mappings failed");
    }
    if (code.empty() || code.size() > 4096 || (code.size() % 4) != 0 ||
        !memory.load(0x10000, code)) {
        throw std::runtime_error("syscall test object code was invalid");
    }
    constexpr char output[] = "Hello from guest ARM64\n";
    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(output), sizeof(output) - 1);
    if (!memory.load(0x20000, bytes)) throw std::runtime_error("guest message loading failed");
    anyios::darwin::SyscallBridge bridge;
    execute_guest(memory, 0x10000, 0x10000 + code.size(), &bridge, 0x20000, bytes.size());
    if (bridge.standard_output() != output || !bridge.standard_error().empty()) {
        throw std::runtime_error("guest Darwin write output mismatch");
    }
}

void write32(std::vector<std::byte>& buffer, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) buffer.at(at + i) = std::byte((value >> (8 * i)) & 255);
}
void write64(std::vector<std::byte>& buffer, std::size_t at, std::uint64_t value) {
    write32(buffer, at, static_cast<std::uint32_t>(value));
    write32(buffer, at + 4, static_cast<std::uint32_t>(value >> 32));
}

void verify_synthetic_executable(const std::vector<std::byte>& code) {
    if (code.empty() || code.size() > 4096 - 0x100) {
        throw std::runtime_error("synthetic executable function is too large");
    }
    std::vector<std::byte> image(4096);
    write32(image, 0, 0xfeedfacf);
    write32(image, 4, 0x0100000c);
    write32(image, 12, 2);
    write32(image, 16, 3);
    write32(image, 20, 120);
    write32(image, 32, 0x19);
    write32(image, 36, 72);
    constexpr char text_name[] = "__TEXT";
    for (std::size_t i = 0; i < sizeof(text_name) - 1; ++i) {
        image[40 + i] = std::byte(text_name[i]);
    }
    write64(image, 32 + 24, 0x10000);
    write64(image, 32 + 32, 4096);
    write64(image, 32 + 48, 4096);
    write32(image, 32 + 56, 5);
    write32(image, 32 + 60, 5);
    write32(image, 104, 0x80000028);
    write32(image, 108, 24);
    write64(image, 112, 0x100);
    write32(image, 128, 0x32);
    write32(image, 132, 24);
    write32(image, 136, 2);
    std::copy(code.begin(), code.end(), image.begin() + 0x100);

    GuestMemory memory(0x10000, 0x30000);
    const auto mapped = anyios::loader::load_static_executable(image, memory);
    if (mapped.entry_address != 0x10100 || mapped.mapped_segments != 1) {
        throw std::runtime_error("synthetic executable entry is incorrect");
    }
    execute_guest(memory, mapped.entry_address, mapped.entry_address + code.size());
}

void verify(const std::vector<std::byte>& code) {
    GuestMemory memory(0x10000, 0x30000);
    const auto rx = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    if (!memory.map(0x10000, 4096, rx) || !memory.map(0x20000, 4096, rw)) {
        throw std::runtime_error("guest memory mapping failed");
    }

    if (code.empty() || code.size() > 4096 || (code.size() % 4) != 0) {
        throw std::runtime_error("guest test function code size is invalid");
    }
    if (!memory.load(0x10000, code)) throw std::runtime_error("guest code loading failed");

    execute_guest(memory, 0x10000, 0x10000 + code.size());
    if (memory.fetch(0x11000)) {
        throw std::runtime_error("unmapped guard page permitted code fetch");
    }
}

}

int main(int argc, char** argv) {
    try {
        std::vector<std::byte> code;
        if (argc == 4 && std::string(argv[1]) == "--linked") {
            verify_real_linked_call(read_owned_binary(argv[2]), read_owned_binary(argv[3]));
            return 0;
        }
        if (argc == 1) {
            code = {std::byte{0x40}, std::byte{0x05}, std::byte{0x80}, std::byte{0xd2},
                    std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}};
        } else if (argc == 2 || (argc == 3 && std::string(argv[1]) == "--syscall")) {
            const bool syscall_test = argc == 3;
            std::ifstream input(syscall_test ? argv[2] : argv[1], std::ios::binary | std::ios::ate);
            if (!input) throw std::runtime_error("cannot open owned Mach-O object");
            const auto length = input.tellg();
            if (length <= 0 || length > 4 * 1024 * 1024) {
                throw std::runtime_error("owned Mach-O fixture file exceeds size limit");
            }
            input.seekg(0);
            std::vector<std::byte> bytes(static_cast<std::size_t>(length));
            if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
                throw std::runtime_error("cannot read owned Mach-O fixture");
            }
            code = anyios::loader::extract_object_function(
                bytes, syscall_test ? "_anyios_syscall_demo" : "_anyios_answer").code;
            if (syscall_test) {
                verify_syscall_object(code);
                std::cout << "Executed ARM64 Darwin SVC guest write via Windows x64: Hello from guest ARM64\n";
                return 0;
            }
        } else {
            throw std::runtime_error("usage: anyios-a64-smoke [owned-mach-o-object | --syscall owned-mach-o-object | --linked RuntimeApp libRuntimeWidget.dylib]");
        }
        verify(code);
        verify_synthetic_executable(code);
        std::cout << "Executed owned ARM64 guest instructions through Dynarmic: x0=42\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ARM64 execution failed: " << error.what() << '\n';
        return 1;
    }
}
