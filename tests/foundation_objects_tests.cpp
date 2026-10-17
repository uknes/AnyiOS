#include <anyios/foundation_objects.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;
using anyios::darwin::GuestFoundationStringStorage;
using anyios::darwin::GuestHleFoundationStringObjects;

void check(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}

void run() {
    GuestMemory memory(0x10000, 0x100000);
    GuestFoundationStringStorage strings(memory, 0x30000);
    GuestHleFoundationStringObjects objects(memory, strings, 0x40000, 0x50000);
    const auto app = objects.create("AppDelegate");
    const auto second = objects.create("AppDelegate");
    check(app && second && *app != *second &&
          *app == 0x40000 && *second == 0x40020 &&
          objects.allocated_count() == 2 && objects.live_count() == 2,
          "HLE object instance identity / distinct addresses failed");
    check(memory.read(*app, 8) == 0x50000 &&
          objects.class_token(*app) == 0x50000 &&
          memory.allowed(*app, 32, Access::write) &&
          !memory.allowed(0x50000, 16, Access::write) &&
          !memory.allowed(0x50000, 16, Access::execute),
          "HLE isa token or guest mapping permissions invalid");
    check(objects.utf16_length(*app) == 11 &&
          objects.code_unit(*app, 0) == 0x41 &&
          objects.code_unit(*app, 3) == 0x44 &&
          !objects.code_unit(*app, 11) &&
          objects.equals(*app, *second) == true,
          "NSString prototype UTF-16 length/equality/indexing failed");

    const auto emoji = objects.create("A\xF0\x9F\x98\x80");
    const auto different = objects.create("AppDelagate");
    check(emoji && objects.utf16_length(*emoji) == 3 &&
          objects.code_unit(*emoji, 1) == 0xD83D &&
          objects.code_unit(*emoji, 2) == 0xDE00 &&
          different && objects.equals(*app, *different) == false,
          "supplementary character / inequality failed");
    check(!objects.create("\xED\xA0\x80") &&
          objects.allocated_count() == 4,
          "malformed UTF-8 allocated an HLE object");

    check(!objects.retain(*app + 8) && !objects.release(*app + 8) &&
          !objects.class_token(*app + 8) &&
          !objects.equals(*app, *app + 8) &&
          !objects.equals(UINT64_MAX, *app) &&
          !objects.is_live(0x10000),
          "forged/interior/out-of-bounds HLE pointer accepted");

    check(objects.retain(*app) && objects.release(*app) &&
          objects.is_live(*app), "retain/release did not preserve live object");
    check(objects.release(*app) && !objects.is_live(*app) &&
          !objects.release(*app) && !objects.retain(*app) &&
          !objects.utf16_length(*app) && !objects.equals(*app, *second) &&
          memory.read(*app, 8) == 0,
          "deallocated HLE object accepted / guest bytes not zeroed");
    const auto fresh = objects.create("AppDelegate");
    check(fresh && *fresh != *app, "ABA alias reused stale guest object address");

    check(memory.write(*second, 0x40000, 8) &&
          !objects.retain(*second) && !objects.release(*second) &&
          !objects.is_live(*second) &&
          !objects.utf16_length(*second) &&
          !objects.equals(*second, *fresh),
          "corrupt isa accepted");
    check(memory.write(*second, 0x50000, 8) &&
          memory.write(*second + 8, 0x1234, 8) &&
          !objects.is_live(*second), "corrupt data pointer accepted");
    check(memory.write(*second + 8, 0x30000, 8) &&
          memory.write(*second + 16, 12, 8) &&
          !objects.is_live(*second), "corrupt length accepted");
    check(memory.write(*second + 16, 11, 8) &&
          memory.write(*second + 24, 0, 8) &&
          !objects.is_live(*second), "corrupt guard accepted");
    // Recover fixture bytes, so explicit ownership release can be tested.
    check(memory.write(*second + 24, 0x31474e525453454cULL, 8) &&
          objects.release(*second), "recovered fixture object release failed");

    while (objects.allocated_count() < GuestHleFoundationStringObjects::kMaxObjects) {
        check(objects.create("AppDelegate").has_value(),
              "valid HLE instance creation failed before capacity");
    }
    check(!objects.create("AppDelegate") &&
          objects.allocated_count() == GuestHleFoundationStringObjects::kMaxObjects &&
          objects.is_live(*fresh),
          "object capacity / existing instance invalidation failed");

    bool collision = false;
    try {
        GuestHleFoundationStringObjects duplicate(memory, strings, 0x40000, 0x60000);
    } catch (const std::invalid_argument&) { collision = true; }
    check(collision && !memory.allowed(0x60000, 8, Access::read),
          "failed construction leaked private class mapping");
    bool class_collision = false;
    try {
        GuestHleFoundationStringObjects duplicate(memory, strings, 0x60000, 0x50000);
    } catch (const std::invalid_argument&) { class_collision = true; }
    check(class_collision && !memory.allowed(0x60000, 8, Access::read),
          "failed class mapping did not rollback guest object arena");
}
} // namespace

int main() {
    try {
        run();
        std::cout << "Private guest HLE Foundation string ownership / isa / UTF-16 bounds passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
