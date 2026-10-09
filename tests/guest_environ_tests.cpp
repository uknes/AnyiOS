#include <anyios/guest_environ.hpp>
#include <anyios/process_bootstrap.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool load_string(anyios::cpu::GuestMemory& mem, std::uint64_t addr, const std::string& text) {
    return mem.load(addr, std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(text.c_str()), text.size() + 1));
}
void run() {
    using anyios::cpu::Access;
    anyios::cpu::GuestMemory memory(0x10000, 0x80000);
    check(memory.map_ios(0x10000, 0x4000,
          anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write)),
          "cannot map guest argument strings");
    check(load_string(memory, 0x10000, "APP_MODE") &&
          load_string(memory, 0x10100, "HOST_NOT_IN_GUEST") &&
          load_string(memory, 0x10200, "PATH") &&
          load_string(memory, 0x10300, "APP_MODE=") &&
          load_string(memory, 0x10400, "BAD\nNAME"), "bad fixture strings");
    constexpr std::array<std::string_view, 1> args{"OriginalApp"};
    constexpr std::array<std::string_view, 2> vars{"APP_MODE=debug", "PATH=/guest/bin"};
    constexpr std::array<std::string_view, 0> apple{};
    const auto state = anyios::loader::prepare_owned_process_stack(
        memory, 0x40000, 0x10000, args, vars, apple);
    const anyios::darwin::GuestEnvironment env(memory, state.envp);
    const auto first = env.lookup(0x10000);
    const auto path = env.lookup(0x10200);
    check(first && *first && path && *path && first != path &&
          memory.read(*first, 1) == 'd' && memory.read(*path, 1) == '/' &&
          env.lookup(0x10000) == first, "getenv did not return stable guest-owned value");
    check(env.lookup(0x10100) == 0 && !env.lookup(0) &&
          !env.lookup(UINT64_MAX) && !env.lookup(0x10300) &&
          !env.lookup(0x10400), "host environment leak or malformed key accepted");
    const anyios::darwin::GuestEnvironment wrong(memory, 0x30000);
    check(!wrong.lookup(0x10000), "unmapped envp accepted");
    constexpr std::array<std::string_view, 0> no_vars{};
    const auto empty = anyios::loader::prepare_owned_process_stack(
        memory, 0x60000, 0x10000, args, no_vars, apple);
    const anyios::darwin::GuestEnvironment cleared(memory, empty.envp);
    check(cleared.lookup(0x10000) == 0, "empty guest envp returned nonnull");
    check(memory.write(state.envp, UINT64_MAX, 8) &&
          !env.lookup(0x10000), "invalid guest envp entry accepted");
}
}
int main() {
    try { run(); std::cout << "Original guest envp getenv contract passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
