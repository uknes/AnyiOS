#include <anyios/guest_memory.hpp>

#include <dynarmic/interface/A64/a64.h>
#include <dynarmic/interface/A64/config.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;

class Callbacks final : public Dynarmic::A64::UserCallbacks {
public:
    explicit Callbacks(GuestMemory& memory) : memory_(memory) {}

    std::optional<std::uint32_t> MemoryReadCode(Dynarmic::A64::VAddr address) override {
        const auto word = memory_.fetch(address);
        if (!word) fault_ = true;
        return word;
    }

    std::uint8_t MemoryRead8(Dynarmic::A64::VAddr addr) override {
        return static_cast<std::uint8_t>(load(addr, 1));
    }
    std::uint16_t MemoryRead16(Dynarmic::A64::VAddr addr) override {
        return static_cast<std::uint16_t>(load(addr, 2));
    }
    std::uint32_t MemoryRead32(Dynarmic::A64::VAddr addr) override {
        return static_cast<std::uint32_t>(load(addr, 4));
    }
    std::uint64_t MemoryRead64(Dynarmic::A64::VAddr addr) override {
        return load(addr, 8);
    }
    Dynarmic::A64::Vector MemoryRead128(Dynarmic::A64::VAddr addr) override {
        if (addr > UINT64_MAX - 8) {
            fault_ = true;
            return {0, 0};
        }
        return {load(addr, 8), load(addr + 8, 8)};
    }
    void MemoryWrite8(Dynarmic::A64::VAddr addr, std::uint8_t val) override {
        store(addr, val, 1);
    }
    void MemoryWrite16(Dynarmic::A64::VAddr addr, std::uint16_t val) override {
        store(addr, val, 2);
    }
    void MemoryWrite32(Dynarmic::A64::VAddr addr, std::uint32_t val) override {
        store(addr, val, 4);
    }
    void MemoryWrite64(Dynarmic::A64::VAddr addr, std::uint64_t val) override {
        store(addr, val, 8);
    }
    void MemoryWrite128(Dynarmic::A64::VAddr addr, Dynarmic::A64::Vector val) override {
        if (addr > UINT64_MAX - 15 || !memory_.allowed(addr, 16, Access::write)) {
            fault_ = true;
            return;
        }
        store(addr, val[0], 8);
        store(addr + 8, val[1], 8);
    }

    void InterpreterFallback(Dynarmic::A64::VAddr, std::size_t) override { fault_ = true; }
    void CallSVC(std::uint32_t) override { fault_ = true; }
    void ExceptionRaised(Dynarmic::A64::VAddr, Dynarmic::A64::Exception) override {
        fault_ = true;
    }
    void AddTicks(std::uint64_t ticks) override { ticks_ += ticks; }
    std::uint64_t GetTicksRemaining() override { return 1; }
    std::uint64_t GetCNTPCT() override { return ticks_; }
    bool failed() const { return fault_; }

private:
    std::uint64_t load(std::uint64_t addr, unsigned width) {
        const auto value = memory_.read(addr, width);
        if (!value) fault_ = true;
        return value.value_or(0);
    }
    void store(std::uint64_t addr, std::uint64_t value, unsigned width) {
        if (!memory_.write(addr, value, width)) fault_ = true;
    }
    GuestMemory& memory_;
    std::uint64_t ticks_ = 0;
    bool fault_ = false;
};

void verify() {
    GuestMemory memory(0x10000, 0x30000);
    const auto rx = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    if (!memory.map(0x10000, 4096, rx) || !memory.map(0x20000, 4096, rw)) {
        throw std::runtime_error("guest memory mapping failed");
    }

    const std::array<std::byte, 8> arm64{
        std::byte{0x40}, std::byte{0x05}, std::byte{0x80}, std::byte{0xd2},
        std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}
    };
    if (!memory.load(0x10000, arm64)) throw std::runtime_error("guest code loading failed");

    Callbacks callbacks(memory);
    Dynarmic::A64::UserConfig config{};
    config.callbacks = &callbacks;
    config.enable_cycle_counting = false;
    Dynarmic::A64::Jit jit(config);
    jit.SetPC(0x10000);
    jit.SetRegister(30, 0x10008);
    for (unsigned i = 0; i < 2; ++i) {
        static_cast<void>(jit.Step());
        if (callbacks.failed()) throw std::runtime_error("guest execution raised a fault");
    }
    if (jit.GetRegister(0) != 42 || jit.GetPC() != 0x10008) {
        throw std::runtime_error("ARM64 code returned an incorrect result");
    }
    if (memory.fetch(0x11000)) {
        throw std::runtime_error("unmapped guard page permitted code fetch");
    }
}

}

int main() {
    try {
        verify();
        std::cout << "Executed ARM64 MOVZ/RET guest code through Dynarmic: x0=42\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ARM64 execution failed: " << error.what() << '\n';
        return 1;
    }
}
