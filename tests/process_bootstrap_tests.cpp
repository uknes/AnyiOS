#include <anyios/process_bootstrap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
std::string read_string(const anyios::cpu::GuestMemory& m, std::uint64_t at) {
    std::string out;
    for (int i = 0; i < 1024; ++i) {
        const auto ch = m.read(at + i, 1);
        if (!ch) throw std::runtime_error("invalid process string pointer");
        if (!*ch) return out;
        out.push_back(static_cast<char>(*ch));
    }
    throw std::runtime_error("unterminated Apple process string");
}
}
int main() {
    try {
        anyios::cpu::GuestMemory memory(0x10000, 0x50000);
        const std::array<std::string_view, 2> argv{"anyios-hello", "guest"};
        const std::array<std::string_view, 1> envp{"ANYIOS_TEST=1"};
        const std::array<std::string_view, 1> apple{"executable_path=/AnyiOS/hello"};
        const auto boot = anyios::loader::prepare_owned_process_stack(
            memory, 0x20000, 0x10000, argv, envp, apple);
        check(boot.argc == 2 && !(boot.sp & 15), "guest argc or 16-byte SP");
        check(memory.read(boot.argv + 16, 8) == 0, "argv terminator");
        check(memory.read(boot.envp + 8, 8) == 0, "envp terminator");
        check(memory.read(boot.apple + 8, 8) == 0, "apple terminator");
        check(read_string(memory, *memory.read(boot.argv, 8)) == "anyios-hello", "argv0");
        check(read_string(memory, *memory.read(boot.argv + 8, 8)) == "guest", "argv1");
        check(read_string(memory, *memory.read(boot.envp, 8)) == "ANYIOS_TEST=1", "envp0");
        check(read_string(memory, *memory.read(boot.apple, 8)) ==
              "executable_path=/AnyiOS/hello", "apple0");
        check(memory.write(boot.sp - 16, 42, 8), "guest stack has descending headroom");
        try {
            (void)anyios::loader::prepare_owned_process_stack(
                memory, 0x34000, 4096, argv, envp, apple);
            throw std::runtime_error("4 KiB guest stack accepted");
        } catch (const anyios::macho::FormatError&) {}
        check(!memory.allowed(0x34000, 4, anyios::cpu::Access::read),
              "invalid startup stack leaked mapping");
        std::cout << "Owned Apple process argv/envp/apple stack layout passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
