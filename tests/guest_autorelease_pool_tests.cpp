#include <anyios/guest_autorelease_pool.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using anyios::cpu::GuestMemory;
using anyios::cpu::Access;
using anyios::darwin::GuestAutoreleasePools;

void check(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
}

void test() {
    GuestMemory memory(0x10000, 0x100000);
    const auto rw = anyios::cpu::bits(Access::read) |
                    anyios::cpu::bits(Access::write);
    check(memory.map_ios(0x10000, 0x4000, rw), "fixture mapping failed");
    GuestAutoreleasePools pools(memory, 0x50000);
    check(!memory.allowed(0x50000, 8, Access::write) &&
          memory.allowed(0x50000, 8, Access::read) &&
          !memory.allowed(0x50000, 8, Access::execute),
          "opaque guest tokens are not read-only data");
    check(pools.finish_thread(1) && !pools.pop(1, 0x50000),
          "empty pool underflow accepted");

    const auto outer = pools.push(1);
    const auto inner = pools.push(1);
    const auto other = pools.push(2);
    check(outer && inner && other && *outer == 0x50000 &&
          *inner == 0x50010 && *other == 0x50020,
          "opaque tokens are not unique and monotonically allocated");
    check(pools.has_open_pools(1) && pools.has_open_pools(2) &&
          !pools.finish_thread(1), "live pool was accepted at thread exit");
    check(!pools.pop(1, *outer) && !pools.pop(2, *inner) &&
          !pools.pop(1, *inner + 1) && !pools.pop(1, UINT64_MAX) &&
          pools.has_open_pools(1),
          "cross-thread, out-of-order or forged token altered pool state");
    check(pools.pop(1, *inner) && pools.pop(1, *outer) &&
          pools.pop(2, *other) && pools.finish_thread(1) &&
          pools.finish_thread(2), "valid nested LIFO drain failed");
    check(!pools.pop(1, *inner) && !pools.pop(1, *outer),
          "replayed tokens were accepted");
    const auto fresh = pools.push(1);
    check(fresh && *fresh != *outer && *fresh != *inner &&
          !pools.pop(1, *outer) && pools.pop(1, *fresh),
          "ABA/reused token accepted after a new push");

    std::vector<std::uint64_t> depth;
    for (std::size_t i = 0; i < GuestAutoreleasePools::kMaxDepth; ++i) {
        const auto token = pools.push(5);
        check(token.has_value(), "valid nested pool was refused");
        depth.push_back(*token);
    }
    const auto before = pools.allocated_tokens();
    check(!pools.push(5) && pools.allocated_tokens() == before,
          "pool depth overflow consumed a token");
    while (!depth.empty()) {
        check(pools.pop(5, depth.back()), "nested pool drain failed");
        depth.pop_back();
    }

    std::vector<std::uint64_t> thread_tokens;
    for (std::uint64_t id = 100; id < 100 + GuestAutoreleasePools::kMaxThreads; ++id) {
        auto token = pools.push(id);
        check(token.has_value(), "registered guest thread refused");
        thread_tokens.push_back(*token);
    }
    const auto threads_before = pools.allocated_tokens();
    check(!pools.push(200) && pools.allocated_tokens() == threads_before,
          "thread capacity overflow consumed a token");
    for (std::uint64_t id = 100; id < 100 + GuestAutoreleasePools::kMaxThreads; ++id) {
        check(pools.pop(id, thread_tokens[static_cast<std::size_t>(id - 100)]),
              "guest thread pool drain failed");
    }
    check(pools.push(200).has_value(), "released guest thread capacity not reusable");
    check(pools.has_open_pools(200), "new guest thread pool missing");

    while (pools.allocated_tokens() < GuestAutoreleasePools::kMaxTokens) {
        const auto token = pools.push(300);
        check(token && pools.pop(300, *token), "bounded token sequence failed");
    }
    check(!pools.push(300) &&
          pools.allocated_tokens() == GuestAutoreleasePools::kMaxTokens,
          "exhausted pool token page was reused");
    check(!pools.pop(300, 0x50000), "expired token accepted at exhaustion");

    bool collision_refused = false;
    try { GuestAutoreleasePools colliding(memory, 0x50000); }
    catch (const std::runtime_error&) { collision_refused = true; }
    check(collision_refused, "overlapping token arena accepted");
    bool unaligned_refused = false;
    try { GuestAutoreleasePools unaligned(memory, 0x54010); }
    catch (const std::runtime_error&) { unaligned_refused = true; }
    check(unaligned_refused, "unaligned token arena accepted");
}
} // namespace

int main() {
    try {
        test();
        std::cout << "Guest empty Objective-C autorelease pool contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
