#include <anyios/guest_os_log.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <span>
namespace {
void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
bool put(anyios::cpu::GuestMemory& m, std::uint64_t a, const std::string& str) {
    return m.load(a, std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(str.c_str()), str.size() + 1));
}
void test() {
    using anyios::cpu::Access;
    anyios::cpu::GuestMemory mem(0x10000, 0x60000);
    check(mem.map_ios(0x10000, 0x4000,
          anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write)),
          "cannot map read-only test strings");
    check(put(mem, 0x10000, "org.wikimedia.wikipedia") &&
          put(mem, 0x11000, "app") &&
          put(mem, 0x12000, "network") &&
          put(mem, 0x13000, ""), "cannot populate guest string table");
    anyios::darwin::GuestOsLogRegistry logs(mem, 0x30000);
    auto a = logs.create(0x10000, 0x11000);
    auto b = logs.create(0x10000, 0x12000);
    check(a && b && *a == 0x30000 && *b == 0x30010 &&
          logs.create(0x10000, 0x11000) == a && logs.size() == 2 &&
          logs.owns(*a) && logs.owns(*b) && !logs.owns(*b + 1) &&
          !mem.allowed(*a, 8, Access::write),
          "tokens not stable/opaque/read-only guest capabilities");
    check(!logs.create(0, 0x11000) && !logs.create(0x10000, 0) &&
          !logs.create(UINT64_MAX, 0x11000) &&
          !logs.create(0x10000, UINT64_MAX) &&
          !logs.create(0x13000, 0x11000) && logs.size() == 2,
          "bad guest pointers or names allocated tokens");
    check(put(mem, 0x13000, std::string(256, 'x')) &&
          !logs.create(0x10000, 0x13000),
          "unterminated/beyond-limit guest string accepted");
    check(mem.write(0x11000, 10, 1) &&
          !logs.create(0x10000, 0x11000),
          "control byte string accepted");
}
}
int main() {
    try { test(); std::cout << "Scoped guest os_log_create contract passed\n"; return 0; }
    catch (const std::exception& err) { std::cerr << err.what() << '\n'; return 1; }
}
