#include <anyios/foundation_autorelease.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using anyios::cpu::GuestMemory;
using anyios::darwin::GuestAutoreleasePools;
using anyios::darwin::GuestFoundationStringStorage;
using anyios::darwin::GuestHleFoundationStringObjects;
using anyios::darwin::GuestFoundationAutoreleaseScopes;

void check(bool success, const char* message) {
    if (!success) throw std::runtime_error(message);
}

void contract() {
    GuestMemory memory(0x10000, 0x100000);
    GuestFoundationStringStorage strings(memory, 0x30000);
    GuestHleFoundationStringObjects objects(memory, strings, 0x40000, 0x50000);
    GuestAutoreleasePools pool_tokens(memory, 0x60000);
    GuestFoundationAutoreleaseScopes scopes(pool_tokens, objects);
    const auto a = objects.create("AppDelegate");
    const auto b = objects.create("A\xF0\x9F\x98\x80");
    check(a && b, "HLE objects not created");
    check(!scopes.enqueue(1, *a) && scopes.finish_thread(1),
          "autorelease accepted without active scope");
    const auto outer = scopes.push(1);
    check(outer && scopes.enqueue(1, *a) &&
          !scopes.enqueue(1, *a), "unretained repeated autorelease allowed");
    const auto inner = scopes.push(1);
    check(inner && *inner != *outer && scopes.enqueue(1, *b) &&
          !scopes.enqueue(2, *b) && scopes.pending() == 2,
          "nested/thread-specific enqueue failed");
    check(!scopes.pop(1, *outer) && !scopes.pop(2, *inner) &&
          scopes.pending() == 2 && objects.is_live(*a) && objects.is_live(*b),
          "wrong-thread/out-of-order pop changed live objects");
    check(scopes.pop(1, *inner) && !objects.is_live(*b) &&
          objects.is_live(*a) && scopes.pending() == 1,
          "inner pool did not release only its own objects");
    check(!scopes.pop(1, *inner) && scopes.pop(1, *outer) &&
          !objects.is_live(*a) && scopes.pending() == 0 &&
          scopes.finish_thread(1), "outer pool drain/replay failed");

    const auto c = objects.create("same");
    check(c && objects.retain(*c), "retained object setup failed");
    const auto first = scopes.push(7);
    check(first && scopes.enqueue(7, *c), "first queued reference rejected");
    const auto second = scopes.push(7);
    check(second && scopes.enqueue(7, *c) &&
          !scopes.enqueue(7, *c), "excess queued references accepted");
    check(scopes.pop(7, *second) && objects.is_live(*c) &&
          objects.reference_count(*c) == 1 &&
          scopes.pop(7, *first) && !objects.is_live(*c),
          "retained shared object drained out of nested order");

    const auto d = objects.create("tamper");
    const auto original_data = d ? memory.read(*d + 8, 8) : std::nullopt;
    check(d && original_data, "tamper fixture setup failed");
    const auto bad_scope = scopes.push(9);
    check(bad_scope && scopes.enqueue(9, *d) &&
          memory.write(*d + 8, 0x1234, 8), "corruption fixture failed");
    check(!scopes.pop(9, *bad_scope) && scopes.pending() == 1 &&
          !scopes.finish_thread(9), "tampered object silently drained");
    check(memory.write(*d + 8, *original_data, 8) &&
          scopes.pop(9, *bad_scope) && !objects.is_live(*d),
          "recoverable validation failure changed pool state");

    const auto e = objects.create("released too soon");
    const auto e_scope = scopes.push(11);
    check(e && e_scope && scopes.enqueue(11, *e) &&
          objects.release(*e) && !scopes.pop(11, *e_scope),
          "prematurely destroyed object caused invalid release");
    // In a fail-closed diagnostic, the abandoned scope stays open.
    check(!scopes.finish_thread(11), "failed drain incorrectly finished thread");
    check(scopes.pending() == 1, "failed drain incorrectly consumed reservation");

    const auto f = objects.create("other thread");
    const auto t1 = scopes.push(20);
    const auto t2 = scopes.push(21);
    check(f && t1 && t2 && scopes.enqueue(21, *f) &&
          !scopes.pop(20, *t2) && scopes.pop(21, *t2) &&
          !objects.is_live(*f) && scopes.pop(20, *t1) &&
          scopes.finish_thread(20) && scopes.finish_thread(21),
          "cross-thread scope isolation failed");
}
} // namespace

int main() {
    try {
        contract();
        std::cout << "Bounded private HLE autorelease scope ownership passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
