#include <anyios/guest_2048.hpp>

#include <anyios/linked_image.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace anyios::gui {
namespace {
constexpr std::uint64_t kGuestStackBase = 0x500000;
constexpr std::uint64_t kGuestStackBytes = 0x10000;
constexpr std::uint64_t kReturnSentinel = 0x700000;
constexpr std::uint32_t kStateMagic = 0x32303438u;

std::vector<std::byte> read_guest(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open owned ARM64 iOS GUI guest: " + path);
    const auto length = input.tellg();
    if (length <= 0 || length > 2 * 1024 * 1024) {
        throw std::runtime_error("invalid owned GUI guest size (2 MiB limit)");
    }
    input.seekg(0);
    std::vector<std::byte> result(static_cast<std::size_t>(length));
    if (!input.read(reinterpret_cast<char*>(result.data()),
                    static_cast<std::streamsize>(result.size()))) {
        throw std::runtime_error("cannot read owned GUI guest");
    }
    return result;
}
}

Guest2048::Guest2048(const std::string& image_path)
    : memory_(0x10000, 8 * 1024 * 1024) {
    auto binary = read_guest(image_path);
    image_ = macho::inspect(binary);
    if (image_.file_type != 2 || image_.is_fat || image_.is_encrypted ||
        !image_.chained_imports.empty() || !image_.has_entry) {
        throw std::runtime_error("interactive guest must be standalone, thin, SDK-free arm64 MH_EXECUTE");
    }
    const auto text = std::find_if(image_.segments.begin(), image_.segments.end(),
        [](const macho::Segment& s) { return s.name == "__TEXT"; });
    if (text == image_.segments.end()) {
        throw std::runtime_error("interactive guest has no __TEXT section");
    }
    original_base_ = text->vm_address;
    const auto image = loader::stage_linked_image(
        binary, memory_, guest_base_, {}, loader::LinkedImageOptions{true, nullptr});
    const auto rw = cpu::bits(cpu::Access::read) | cpu::bits(cpu::Access::write);
    if (!memory_.map_ios(kGuestStackBase, kGuestStackBytes, rw)) {
        throw std::runtime_error("cannot create bounded guest stack");
    }
    state_address_ = address("_anyios_2048_state");
    move_entry_ = address("_anyios_2048_move");
    reset_entry_ = address("_anyios_2048_reset");
    if (!memory_.allowed(state_address_, sizeof(Guest2048State), cpu::Access::read) ||
        !memory_.fetch(move_entry_) || !memory_.fetch(reset_entry_) ||
        !memory_.fetch(image.guest_entry)) {
        throw std::runtime_error("GUI guest exported function/state symbols are unmapped");
    }
    cpu_ = cpu::make_dynarmic_backend(memory_);
    if (invoke(image.guest_entry, 0, 800000) != 0) {
        throw std::runtime_error("project-owned ARM64 guest main returned error");
    }
    auto current = snapshot();
    unsigned populated = 0;
    for (auto tile : current.tiles) if (tile) ++populated;
    if (populated != 2 || current.score != 0) {
        throw std::runtime_error("ARM64 guest board initialization invalid");
    }
}

std::uint64_t Guest2048::address(std::string_view symbol) const {
    for (const auto& item : image_.symbols) {
        if (item.name != symbol) continue;
        if (item.value < original_base_ ||
            item.value - original_base_ > UINT64_MAX - guest_base_) {
            throw std::runtime_error("ARM64 exported symbol address overflow");
        }
        return guest_base_ + (item.value - original_base_);
    }
    throw std::runtime_error("ARM64 guest missing required exported symbol " +
                             std::string(symbol));
}

std::uint64_t Guest2048::invoke(std::uint64_t entry, std::uint64_t x0,
                                std::uint64_t budget) {
    if (!cpu_ || !memory_.fetch(entry) || budget == 0 || budget > 2000000) {
        throw std::runtime_error("invalid entry or instruction budget for guest call");
    }
    cpu::CpuState state{};
    state.pc = entry;
    state.sp = kGuestStackBase + kGuestStackBytes;
    state.x[0] = x0;
    state.x[30] = kReturnSentinel;
    cpu_->set_state(state);
    const auto event = cpu_->run_until_event(budget, kReturnSentinel);
    const auto after = cpu_->state();
    if (event.kind != cpu::CpuEventKind::returned) {
        throw std::runtime_error("ARM64 guest function did not return: " + event.diagnostic +
                                 ", PC=" + std::to_string(after.pc));
    }
    if (after.sp != state.sp) {
        throw std::runtime_error("ARM64 guest function corrupted its 16-byte aligned stack");
    }
    return after.x[0];
}

Guest2048State Guest2048::snapshot() const {
    Guest2048State result{};
    if (!memory_.copy_from(state_address_, std::span<std::byte>(
           reinterpret_cast<std::byte*>(&result), sizeof(result))) ||
        result.magic != kStateMagic || result.version != 1) {
        throw std::runtime_error("invalid or inaccessible guest 2048 state");
    }
    for (auto tile : result.tiles) {
        if (tile != 0 && (tile < 2 || (tile & (tile - 1)) != 0)) {
            throw std::runtime_error("guest game state contains impossible tile");
        }
    }
    return result;
}

bool Guest2048::move(unsigned direction) {
    if (direction > 3) {
        throw std::invalid_argument("2048 guest input direction must be 0..3");
    }
    const auto before = snapshot();
    const auto result = invoke(move_entry_, direction, 1200000);
    if (result > 1) {
        throw std::runtime_error("ARM64 guest move returned unknown result");
    }
    const auto after = snapshot();
    if (result && (after.moves != before.moves + 1 ||
                   after.best < before.best)) {
        throw std::runtime_error("ARM64 guest move did not update board coherently");
    }
    if (!result && after.moves != before.moves) {
        throw std::runtime_error("ARM64 guest rejected move mutated its count");
    }
    return result != 0;
}

void Guest2048::reset(std::uint32_t seed) {
    auto previous = snapshot();
    invoke(reset_entry_, seed, 800000);
    const auto state = snapshot();
    unsigned populated = 0;
    for (auto tile : state.tiles) if (tile) ++populated;
    if (state.moves != 0 || state.score != 0 ||
        state.best < previous.best || populated != 2) {
        throw std::runtime_error("ARM64 guest reset returned invalid board");
    }
}
} // namespace anyios::gui
