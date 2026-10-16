#include <anyios/foundation_string.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;
using anyios::darwin::GuestFoundationStringStorage;

void check(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}

void run() {
    GuestMemory memory(0x10000, 0x100000);
    GuestFoundationStringStorage strings(memory, 0x50000);
    const auto ascii = strings.stage_utf8("AppDelegate");
    check(ascii && ascii->guest_data == 0x50000 && ascii->code_units == 11,
          "validated class name UTF-16 snapshot failed");
    check(!memory.allowed(ascii->guest_data, 2, Access::write) &&
          !memory.allowed(ascii->guest_data, 2, Access::execute) &&
          memory.allowed(ascii->guest_data, 2, Access::read),
          "guest UTF-16 snapshot page must be non-writable/non-executable");
    check(memory.read(ascii->guest_data, 2) == static_cast<std::uint64_t>('A') &&
          memory.read(ascii->guest_data + 2 * ascii->code_units, 2) == 0 &&
          strings.length(ascii->guest_data) == 11 &&
          strings.code_unit(ascii->guest_data, 3) == 'D',
          "UTF-16LE contents or length incorrect");
    check(strings.stage_utf8("AppDelegate")->guest_data == ascii->guest_data &&
          strings.size() == 1, "repeated string did not preserve snapshot identity");

    // U+00E9, U+20AC, and U+1F600 are respectively 1, 1 and 2 UTF-16 units.
    const std::string unicode = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    const auto wide = strings.stage_utf8(unicode);
    check(wide && wide->code_units == 5 &&
          strings.code_unit(wide->guest_data, 0) == 0x0041 &&
          strings.code_unit(wide->guest_data, 1) == 0x00E9 &&
          strings.code_unit(wide->guest_data, 2) == 0x20AC &&
          strings.code_unit(wide->guest_data, 3) == 0xD83D &&
          strings.code_unit(wide->guest_data, 4) == 0xDE00,
          "UTF-8 to UTF-16 surrogate/length conversion failed");
    const auto empty = strings.stage_utf8("");
    check(empty && empty->code_units == 0 &&
          memory.read(empty->guest_data, 2) == 0,
          "empty string representation failed");
    check(!strings.length(ascii->guest_data + 2) &&
          !strings.length(UINT64_MAX) &&
          !strings.code_unit(ascii->guest_data, 11) &&
          !strings.code_unit(0x50000 + 32 * GuestFoundationStringStorage::kStride, 0),
          "guest interior, forged, or past-end string pointers accepted");

    const std::vector<std::string> bad{
        "\x80", "\xC0\xAF", "\xE0\x80\xAF", "\xED\xA0\x80",
        "\xF4\x90\x80\x80", "\xF5\x80\x80\x80",
        "\xF0\x9F", "\xE2\x28\xA1", std::string(256, 'b')
    };
    const auto stable = strings.size();
    for (const auto& value : bad) {
        check(!strings.stage_utf8(value), "invalid UTF-8 or oversized string accepted");
    }
    check(strings.size() == stable, "invalid UTF-8 consumed guest string slots");
    const auto exact_limit = strings.stage_utf8(std::string(
        GuestFoundationStringStorage::kMaxCodeUnits, 'x'));
    check(exact_limit && exact_limit->code_units == 255 &&
          memory.read(exact_limit->guest_data + 510, 2) == 0,
          "boundary UTF-16 storage/terminator failed");
    while (strings.size() < GuestFoundationStringStorage::kMaxEntries) {
        auto value = strings.stage_utf8("unique-" + std::to_string(strings.size()));
        check(value, "guest string store refused a valid unique entry");
    }
    check(!strings.stage_utf8("overflow") &&
          strings.size() == GuestFoundationStringStorage::kMaxEntries &&
          strings.stage_utf8("AppDelegate")->guest_data == ascii->guest_data,
          "capacity exceeded or existing snapshot invalidated");

    bool overlap_refused = false;
    try { GuestFoundationStringStorage collision(memory, 0x50000); }
    catch (const std::runtime_error&) { overlap_refused = true; }
    check(overlap_refused, "overlapping Foundation snapshot arena accepted");
    bool misaligned_refused = false;
    try { GuestFoundationStringStorage invalid(memory, 0x54010); }
    catch (const std::runtime_error&) { misaligned_refused = true; }
    check(misaligned_refused, "misaligned Foundation snapshot arena accepted");
}
} // namespace

int main() {
    try {
        run();
        std::cout << "Bounded guest Foundation UTF-16 snapshot contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
