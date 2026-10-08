#include <anyios/native_a64.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#else
#error "Native A64 fixtures are supported on Windows and Linux only"
#endif
#if !defined(_M_ARM64) && !defined(__aarch64__)
#error "Native A64 fixtures require an ARM64 compiler"
#endif

namespace anyios::cpu {
namespace {
constexpr std::size_t page_size = 4096;

std::uint32_t word(std::span<const std::byte> bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= std::uint32_t(std::to_integer<std::uint8_t>(bytes[at + i])) << (i * 8);
    }
    return value;
}

class ExecutablePage {
public:
    ExecutablePage() {
#if defined(_WIN32)
        address_ = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!address_) throw std::runtime_error("VirtualAlloc failed");
#else
        address_ = mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (address_ == MAP_FAILED) {
            address_ = nullptr;
            throw std::runtime_error("mmap failed");
        }
#endif
    }
    ExecutablePage(const ExecutablePage&) = delete;
    ExecutablePage& operator=(const ExecutablePage&) = delete;
    ~ExecutablePage() {
        if (!address_) return;
#if defined(_WIN32)
        VirtualFree(address_, 0, MEM_RELEASE);
#else
        munmap(address_, page_size);
#endif
    }
    void* data() const { return address_; }
    void seal() {
#if defined(_WIN32)
        DWORD previous = 0;
        if (!VirtualProtect(address_, page_size, PAGE_EXECUTE_READ, &previous)) {
            throw std::runtime_error("VirtualProtect RX failed");
        }
        if (!FlushInstructionCache(GetCurrentProcess(), address_, page_size)) {
            throw std::runtime_error("FlushInstructionCache failed");
        }
#else
        if (mprotect(address_, page_size, PROT_READ | PROT_EXEC) != 0) {
            throw std::runtime_error("mprotect RX failed");
        }
        auto* begin = static_cast<char*>(address_);
        __builtin___clear_cache(begin, begin + page_size);
#endif
    }
private:
    void* address_ = nullptr;
};
}

std::uint64_t execute_owned_arm64_fixture(std::span<const std::byte> instructions) {
    if (instructions.size() != 8 ||
        (word(instructions, 0) != 0xd2800540U &&
         word(instructions, 0) != 0x52800540U) ||
        word(instructions, 4) != 0xd65f03c0U) {
        throw std::invalid_argument("only the owned MOVZ #42 / RET fixture is supported");
    }
    ExecutablePage page;
    std::memcpy(page.data(), instructions.data(), instructions.size());
    page.seal();
    using FixtureFunction = std::uint64_t (*)();
    const auto function = reinterpret_cast<FixtureFunction>(page.data());
    return function();
}
}
