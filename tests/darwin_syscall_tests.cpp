#include <anyios/darwin_syscall.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool passed, std::string_view reason) {
    if (!passed) throw std::runtime_error(std::string(reason));
}

void test() {
    using anyios::cpu::Access;
    using anyios::cpu::GuestMemory;
    GuestMemory memory(0x10000, 0x3000);
    require(memory.map(0x10000, 4096, anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write)), "map data");
    require(memory.map(0x11000, 4096, anyios::cpu::bits(Access::execute)), "map execute");
    constexpr char message[] = "AnyiOS syscall boundary\n";
    const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(message), sizeof(message) - 1);
    require(memory.load(0x10020, bytes), "seed input");
    anyios::darwin::SyscallBridge bridge;
    auto result = bridge.dispatch(0x80, 4, {1, 0x10020, bytes.size()}, memory);
    require(result.supported && !result.carry && result.value == bytes.size(), "write result");
    require(bridge.standard_output() == message, "capture stdout");

    result = bridge.dispatch(0x80, 4, {2, 0x10020, bytes.size()}, memory);
    require(result.supported && !result.carry && bridge.standard_error() == message, "capture stderr");
    result = bridge.dispatch(0x80, 4, {3, 0x10020, bytes.size()}, memory);
    require(result.supported && result.carry && result.value == 9, "bad fd");
    result = bridge.dispatch(0x80, 4, {1, 0x11000, bytes.size()}, memory);
    require(result.supported && result.carry && result.value == 14, "execute page not readable");
    result = bridge.dispatch(0x80, 4, {1, UINT64_MAX - 4, 16}, memory);
    require(result.supported && result.carry && result.value == 14, "overflow address");
    result = bridge.dispatch(0x80, 4, {1, 0x10000, 65537}, memory);
    require(result.supported && result.carry && result.value == 22, "unbounded data");
    result = bridge.dispatch(0x80, 4, {1, 0x10000, 0}, memory);
    require(result.supported && !result.carry && result.value == 0, "zero-length write");
    require(bridge.standard_output() == message, "failed writes changed output");

    result = bridge.dispatch(0x80, 1, {42, 0, 0}, memory);
    require(result.supported && result.exited && result.value == 42, "exit request");
    result = bridge.dispatch(0x80, 32767, {0, 0, 0}, memory);
    require(!result.supported && result.diagnostic.find("32767") != std::string::npos, "unsupported trap");
    result = bridge.dispatch(0, 4, {1, 0x10020, bytes.size()}, memory);
    require(!result.supported && result.diagnostic.find("SVC") != std::string::npos, "wrong immediate");
}
}
int main() {
    try {
        test();
        std::cout << "Darwin guest write/exit trap boundary checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
