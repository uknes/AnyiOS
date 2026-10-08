#include <anyios/guest_memory.hpp>
#include <anyios/cpu_backend.hpp>
#include <anyios/abi_thunk.hpp>
#include <anyios/object_code.hpp>
#include <anyios/executable.hpp>
#include <anyios/linked_pair.hpp>
#include <anyios/macho.hpp>
#include <anyios/libsystem_shim.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/process_bootstrap.hpp>
#include <anyios/darwin_syscall.hpp>


#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
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


void verify_sdkfree_libsystem(const std::vector<std::byte>& file, bool complete_process = false) {
    constexpr std::uint64_t stubs_base = 0x80000;
    constexpr std::uint64_t heap_base = 0x200000;
    constexpr std::uint64_t stack_base = 0x300000;
    constexpr std::uint64_t sentinel = 0x400000;
    struct Stub {
        const char* name;
        std::uint32_t service;
        std::uint64_t guest_address;
    };
    constexpr std::array<Stub, 3> stubs{{
        {"_malloc", 0x1000, stubs_base},
        {"_write", 4, stubs_base + 16},
        {"_exit", 1, stubs_base + 32}
    }};
    GuestMemory memory(0x10000, 4 * 1024 * 1024);
    const auto image = anyios::macho::inspect(file);
    if (image.file_type != 2 || image.chained_imports.size() != stubs.size()) {
        throw std::runtime_error("SDK-free libSystem fixture has unsupported imports");
    }
    std::vector<std::uint64_t> resolved;
    for (const auto& name : image.chained_imports) {
        const auto found = std::find_if(stubs.begin(), stubs.end(),
            [&](const Stub& stub) { return name == stub.name; });
        if (found == stubs.end()) {
            throw std::runtime_error("unknown guest libSystem symbol: " + name);
        }
        resolved.push_back(found->guest_address);
    }
    const auto mapped = anyios::loader::stage_linked_image(
        file, memory, 0x10000, resolved,
        anyios::loader::LinkedImageOptions{true, nullptr});
    const auto rx = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    if (!memory.map_ios(stubs_base, 0x4000, rx) ||
        !memory.map_ios(stack_base, 0x10000, rw)) {
        throw std::runtime_error("owned libSystem stub/stack guest mapping failed");
    }
    std::array<std::byte, 0x4000> code{};
    for (const auto& stub : stubs) {
        const auto at = static_cast<std::size_t>(stub.guest_address - stubs_base);
        const auto mov = 0xd2800010U | (stub.service << 5);
        const std::array<std::uint32_t, 3> instructions{
            mov, 0xd4001001U, 0xd65f03c0U
        };
        for (std::size_t i = 0; i < instructions.size(); ++i) {
            for (unsigned byte = 0; byte < 4; ++byte) {
                code[at + i * 4 + byte] =
                    std::byte((instructions[i] >> (byte * 8)) & 0xff);
            }
        }
    }
    if (!memory.load(stubs_base, code)) {
        throw std::runtime_error("could not populate registered ARM64 thunk page");
    }
    anyios::darwin::LibSystemShim libsystem(memory, heap_base, 0x10000);
    auto backend = anyios::cpu::make_dynarmic_backend(memory);
    anyios::cpu::CpuState guest{};
    guest.pc = mapped.guest_entry;
    guest.sp = stack_base + 0x10000;
    guest.x[30] = sentinel;
    if (complete_process) {
        const std::array<std::string_view, 2> args{"anyios-hello", "guest"};
        const std::array<std::string_view, 1> environment{"ANYIOS_TEST=1"};
        const std::array<std::string_view, 1> apple{
            "executable_path=/AnyiOS/hello"
        };
        const auto process = anyios::loader::prepare_owned_process_stack(
            memory, stack_base + 0x20000, 0x10000, args, environment, apple);
        guest.sp = process.sp;
        guest.x[0] = process.argc;
        guest.x[1] = process.argv;
        guest.x[2] = process.envp;
        guest.x[3] = process.apple;
        backend->set_state(guest);
        const auto initializers = anyios::loader::find_owned_module_initializers(
            image, memory, mapped.guest_base);
        if (initializers.size() != 1) {
            throw std::runtime_error("owned hello process requires one real __mod_init_func");
        }
        const std::array<std::uint64_t, 4> initializer_args{
            process.argc, process.argv, process.envp, process.apple
        };
        for (const auto init : initializers) {
            static_cast<void>(anyios::abi::invoke_guest_callback(
                *backend, init, initializer_args, sentinel, 2048));
        }
    }
    backend->set_state(guest);
    for (unsigned event_count = 0; event_count < 24; ++event_count) {
        const auto event = backend->run_until_event(100000, sentinel);
        guest = backend->state();
        if (event.kind != anyios::cpu::CpuEventKind::svc ||
            event.svc_immediate != 0x80) {
            throw std::runtime_error("guest libc test did not reach expected SVC: " +
                                     event.diagnostic);
        }
        const auto stub = std::find_if(stubs.begin(), stubs.end(),
            [&](const Stub& candidate) {
                return guest.pc == candidate.guest_address + 8 &&
                       guest.x[16] == candidate.service;
            });
        if (stub == stubs.end()) {
            throw std::runtime_error("unexpected/unregistered libSystem SVC origin");
        }
        const auto result = libsystem.invoke(stub->name,
            {guest.x[0], guest.x[1], guest.x[2]});
        if (result.exited) {
            const auto expected_exit = complete_process ? 23U : 0U;
            const std::string expected_output = complete_process ? "hello\n" : "OK";
            if (result.value != expected_exit || libsystem.output() != expected_output) {
                throw std::runtime_error("owned Apple process wrong exit code or captured output");
            }
            if (complete_process) {
                std::cout << "Executed owned iOS LC_MAIN process with initializer and startup vectors: hello / exit 23\n";
            } else {
                std::cout << "Executed SDK-free iOS _malloc/_write/_exit through Dynarmic: OK\n";
            }
            return;
        }
        guest.x[0] = result.value;
        backend->set_state(guest);
    }
    throw std::runtime_error("owned libSystem fixture exhausted event budget");
}

void verify_unsupported_tls_registers() {
    const auto rx = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::execute);
    for (const auto instruction : {0xd53bd060U, 0xd51bd060U}) {
        GuestMemory memory(0x10000, 0x4000);
        if (!memory.map(0x10000, 4096, rx)) {
            throw std::runtime_error("owned TLS system-register test map failed");
        }
        std::array<std::byte, 4> bytes{};
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = std::byte((instruction >> (8 * i)) & 0xff);
        if (!memory.load(0x10000, bytes)) {
            throw std::runtime_error("TLS system-register test could not load instruction");
        }
        auto backend = anyios::cpu::make_dynarmic_backend(memory);
        anyios::cpu::CpuState state{};
        state.pc = 0x10000;
        backend->set_state(state);
        const auto event = backend->step();
        if (event.kind != anyios::cpu::CpuEventKind::unsupported ||
            backend->state().pc != 0x10000) {
            throw std::runtime_error("unimplemented Darwin TLS MRS/MSR executed silently");
        }
    }
}

void verify_guest_thread_tpidrro() {
    const auto rx = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    GuestMemory memory(0x10000, 0x40000);
    if (!memory.map_ios(0x10000, 0x4000, rx) ||
        !memory.map_ios(0x20000, 0x4000, rw) ||
        !memory.map_ios(0x24000, 0x4000, rw)) {
        throw std::runtime_error("guest TLS page mapping failed");
    }
    // Authored instructions: mrs x0, TPIDRRO_EL0; ldr x0, [x0]; msr TPIDRRO_EL0, x0
    constexpr std::array<std::uint32_t, 3> instructions{
        0xd53bd060u, 0xf9400000u, 0xd51bd060u
    };
    std::array<std::byte, instructions.size() * 4> code{};
    for (std::size_t i = 0; i < instructions.size(); ++i) {
        for (unsigned b = 0; b < 4; ++b) {
            code[i * 4 + b] = std::byte((instructions[i] >> (8 * b)) & 255);
        }
    }
    if (!memory.load(0x10000, code) ||
        !memory.write(0x20000, 0x11223344, 8) ||
        !memory.write(0x24000, 0x55667788, 8)) {
        throw std::runtime_error("guest TLS setup failed");
    }
    auto backend = anyios::cpu::make_dynarmic_backend(memory);
    anyios::cpu::CpuState one{}, two{};
    one.pc = two.pc = 0x10000;
    one.guest_thread_id = 1;
    two.guest_thread_id = 2;
    one.tpidrro_el0 = 0x20000;
    two.tpidrro_el0 = 0x24000;
    one.tpidrro_valid = two.tpidrro_valid = true;
    backend->set_state(one);
    if (backend->step().kind != anyios::cpu::CpuEventKind::stepped) {
        throw std::runtime_error("first guest thread TPIDRRO MRS failed");
    }
    one = backend->state();
    if (one.x[0] != 0x20000) {
        throw std::runtime_error("first guest thread TPIDRRO pointer mismatch");
    }
    backend->set_state(two);
    if (backend->step().kind != anyios::cpu::CpuEventKind::stepped ||
        backend->step().kind != anyios::cpu::CpuEventKind::stepped ||
        backend->state().x[0] != 0x55667788) {
        throw std::runtime_error("second guest thread TLS data read failed");
    }
    backend->set_state(one);
    if (backend->step().kind != anyios::cpu::CpuEventKind::stepped ||
        backend->state().x[0] != 0x11223344) {
        throw std::runtime_error("first guest thread TLS data was not isolated");
    }
    auto invalid = one;
    invalid.tpidrro_el0 = 0x30000;
    try {
        backend->set_state(invalid);
        throw std::runtime_error("unmapped guest TLS pointer was accepted");
    } catch (const std::invalid_argument&) { }
    one = backend->state();
    one.pc = 0x10008;
    backend->set_state(one);
    if (backend->step().kind != anyios::cpu::CpuEventKind::unsupported ||
        backend->state().pc != 0x10008) {
        throw std::runtime_error("guest attempted write to TPIDRRO was not refused");
    }
    std::cout << "Dynarmic TPIDRRO guest thread switch and native-host pointer isolation passed\\n";
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
    {
        auto backend = anyios::cpu::make_dynarmic_backend(memory);
        anyios::cpu::CpuState state{};
        state.pc = 0x10000;
        state.sp = 0x20000;
        state.x[18] = 0x77889900ULL;
        for (unsigned i = 19; i <= 29; ++i) state.x[i] = 0x91000 + i;
        backend->set_state(state);
        const std::array<std::uint64_t, 1> args{99};
        if (anyios::abi::invoke_guest_callback(
                *backend, 0x10000, args, 0x13000, 16) != 42 ||
            backend->state().x[18] != state.x[18] ||
            backend->state().x[19] != state.x[19] ||
            backend->state().sp != state.sp) {
            throw std::runtime_error("translated guest callback ABI state check failed");
        }
    }
    if (memory.fetch(0x11000)) {
        throw std::runtime_error("unmapped guard page permitted code fetch");
    }
}

}

int main(int argc, char** argv) {
    try {
        std::vector<std::byte> code;
        verify_unsupported_tls_registers();
        verify_guest_thread_tpidrro();
        if (argc == 3 && std::string(argv[1]) == "--hello") {
            verify_sdkfree_libsystem(read_owned_binary(argv[2]), true);
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--libsystem") {
            verify_sdkfree_libsystem(read_owned_binary(argv[2]));
            return 0;
        }
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
