#include <anyios/svc_scan.hpp>
#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {
void rejects(std::span<const std::byte> data) {
    try { anyios::cpu::reject_svc_in_executable_mapping(data); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("unsafe ARM64 executable page passed SVC scan");
}
}
int main() {
    try {
        std::array<std::byte, 16384> image{};
        anyios::cpu::reject_svc_in_executable_mapping(image);
        constexpr std::array<std::byte, 4> svc{
            std::byte{0x01}, std::byte{0x10}, std::byte{0x00}, std::byte{0xd4}
        };
        for (std::size_t i = 0; i < svc.size(); ++i) image[16+i] = svc[i];
        rejects(image);
        for (std::size_t i = 0; i < svc.size(); ++i) image[16+i] = std::byte{0};
        // A literal pool indistinguishable from an opcode also fails closed.
        for (std::size_t i = 0; i < svc.size(); ++i) image[8192+i] = svc[i];
        rejects(image);
        for (std::size_t i = 0; i < svc.size(); ++i) image[8192+i] = std::byte{0};
        anyios::cpu::reject_svc_in_executable_mapping(image);
        rejects(std::span<const std::byte>(image).first(4095));
        rejects(std::span<const std::byte>(image).first(0));
        std::cout << "SVC scan refuses instruction and literal-pool lookalike\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
