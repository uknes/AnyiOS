#pragma once
#include <anyios/fixup_plan.hpp>
namespace anyios::dyld {
// Addresses must be guest targets resolved in eager-site then lazy-site order.
// The caller validates their provenance; no symbol lookup is fabricated here.
// Lazy pointers are bound eagerly. Weak coalescing/threaded binds are refused.
std::vector<FixupPatch> plan_legacy_fixups(
    std::span<const std::byte> file, const macho::Image& image,
    std::span<const std::uint64_t> resolved_targets);
}
