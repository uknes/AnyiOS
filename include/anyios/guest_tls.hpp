#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace anyios::darwin {

// Owned-fixture-only TLV model. All addresses are guest virtual addresses.
// A module's __thread_data and __thread_bss are copied once per emulated thread.
// Unknown descriptors, untrusted TLV layouts, and unsupported teardown fail closed.
class GuestTls final {
public:
    GuestTls(cpu::GuestMemory& memory, std::uint64_t base, std::size_t capacity);

    // Accept only staged/validated Mach-O metadata. Descriptor word 3 is an
    // image-template byte offset, not a relocated pointer. Returns no host pointers.
    void register_module(const macho::Image& image, std::uint64_t guest_base);

    // TPIDRRO_EL0 points to a stable, read-only-by-convention guest thread header.
    std::uint64_t thread_register(std::uint64_t thread_id);

    // ABI for the owned __tlv_bootstrap/_tlv_get_addr call:
    // x0 = registered descriptor guest address, x0 = variable guest address.
    std::uint64_t get_address(std::uint64_t thread_id, std::uint64_t descriptor);

    // No guest destructors are supported. A thread with live TLV allocations
    // cannot be destroyed silently.
    void finish_thread(std::uint64_t thread_id);

private:
    struct Variable {
        std::uint64_t descriptor;
        std::uint64_t module_offset;
    };
    struct Module {
        std::vector<std::byte> initial;
        std::vector<Variable> variables;
    };

    std::uint64_t new_page(std::span<const std::byte> initial);
    cpu::GuestMemory& memory_;
    std::uint64_t next_;
    std::uint64_t end_;
    std::vector<Module> modules_;
    std::map<std::uint64_t, std::uint64_t> headers_;
    std::map<std::pair<std::uint64_t, std::size_t>, std::uint64_t> thread_modules_;
};

} // namespace anyios::darwin
