#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace anyios::cpu {
// Restricted to the project-owned two-instruction fixture, never untrusted guest code.
std::uint64_t execute_owned_arm64_fixture(std::span<const std::byte> instructions);
}
