#pragma once
#include <anyios/cpu_backend.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace anyios::gui {

// Project-owned ARM64 iPhoneOS 2048 guest ABI, not Apple's UIKit API and
// not the original danqing/2048 Objective-C application.
struct Guest2048State {
    std::uint32_t magic;
    std::uint32_t version;
    std::array<std::uint32_t, 16> tiles;
    std::uint32_t score;
    std::uint32_t best;
    std::uint32_t moves;
    std::uint32_t won;
    std::uint32_t game_over;
    std::uint32_t rng;
};
static_assert(sizeof(Guest2048State) == 96);

class Guest2048 {
public:
    explicit Guest2048(const std::string& image_path);
    Guest2048State snapshot() const;
    bool move(unsigned direction); // up, left, down, right
    void reset(std::uint32_t seed);
    std::string image_sha256_hint() const { return "see guest-receipt.json"; }

private:
    std::uint64_t address(std::string_view symbol) const;
    std::uint64_t invoke(std::uint64_t entry, std::uint64_t x0,
                         std::uint64_t budget);
    cpu::GuestMemory memory_;
    macho::Image image_;
    std::uint64_t guest_base_ = 0x10000;
    std::uint64_t original_base_ = 0;
    std::uint64_t state_address_ = 0;
    std::uint64_t move_entry_ = 0;
    std::uint64_t reset_entry_ = 0;
    std::unique_ptr<cpu::CpuBackend> cpu_;
};

} // namespace anyios::gui
