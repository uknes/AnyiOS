#include <anyios/svc_scan.hpp>
#include <cstdint>
#include <stdexcept>

namespace anyios::cpu {
void reject_svc_in_executable_mapping(std::span<const std::byte> bytes) {
    if (bytes.empty() || bytes.size() % 4 != 0) {
        throw std::invalid_argument("ARM64 executable mapping requires complete words");
    }
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        std::uint32_t instruction = 0;
        for (unsigned i = 0; i < 4; ++i) {
            instruction |= std::uint32_t(
                std::to_integer<std::uint8_t>(bytes[offset + i])) << (8 * i);
        }
        if ((instruction & 0xffe0001fU) == 0xd4000001U) {
            throw std::invalid_argument("possible Darwin SVC in executable mapping");
        }
    }
}
}
