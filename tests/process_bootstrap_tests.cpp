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
        {
            anyios::cpu::GuestMemory code(0x10000, 0x30000);
            const auto r = anyios::cpu::bits(anyios::cpu::Access::read);
            const auto rx = r | anyios::cpu::bits(anyios::cpu::Access::execute);
            check(code.map_ios(0x10000, 0x4000, rx), "constructor text map");
            check(code.map_ios(0x14000, 0x4000, r), "constructor data map");
            std::array<std::byte, 4> offset{std::byte{0x00}, std::byte{0x01},
                                            std::byte{0}, std::byte{0}};
            check(code.load(0x10080, offset), "modern initializer offset");
            std::array<std::byte, 4> ret{std::byte{0xc0}, std::byte{0x03},
                                         std::byte{0x5f}, std::byte{0xd6}};
            check(code.load(0x10100, ret), "constructor function code");
            anyios::macho::Image modern;
            modern.segments.push_back({"__TEXT", 0x100000000ULL,
                                       0x4000, 0, 0x4000, 0, 5, 5});
            modern.sections.push_back({"__init_offsets", "__TEXT",
                                       0x100000080ULL, 4, 0x80, 0, false});
            const auto offsets = anyios::loader::find_owned_module_initializers(
                modern, code, 0x10000);
            check(offsets.size() == 1 && offsets[0] == 0x10100,
                  "modern __init_offsets constructor target");
            modern.sections.clear();
            modern.sections.push_back({"__mod_init_func", "__DATA",
                                       0x100004080ULL, 8, 0x4080, 0, false});
            check(code.write(0x14080, 0x10100, 8) == false,
                  "read-only constructor page should refuse guest writes");
            std::array<std::byte, 8> old_ptr{
                std::byte{0x00}, std::byte{0x01}, std::byte{0x01}, std::byte{0},
                std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}
            };
            check(code.load(0x14080, old_ptr), "legacy initializer pointer");
            const auto pointers = anyios::loader::find_owned_module_initializers(
                modern, code, 0x10000);
            check(pointers.size() == 1 && pointers[0] == 0x10100,
                  "legacy __mod_init_func constructor target");
        }

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
