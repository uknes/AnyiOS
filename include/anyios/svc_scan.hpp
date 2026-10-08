#pragma once
#include <cstddef>
#include <span>

namespace anyios::cpu {
// Conservative trusted-fixture preflight: every SVC-looking 32-bit word rejects.
void reject_svc_in_executable_mapping(std::span<const std::byte> bytes);
}
