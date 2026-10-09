#include <anyios/guest_memory.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Never execute an untrusted external application's instructions.
// This tool stages relocation metadata against non-executable, unresolved
// placeholder addresses to locate the first loader boundary. The target app
// is NOT runnable even if staging succeeds.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: anyios-app-stage-probe <ios-arm64-Mach-O>\n";
        return 2;
    }
    try {
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("cannot open external Mach-O");
        const auto length = input.tellg();
        if (length <= 0 || length > 32 * 1024 * 1024) {
            throw std::runtime_error("untrusted app exceeds 32 MiB stage limit");
        }
        input.seekg(0);
        std::vector<std::byte> data(static_cast<std::size_t>(length));
        if (!input.read(reinterpret_cast<char*>(data.data()),
                        static_cast<std::streamsize>(data.size()))) {
            throw std::runtime_error("external Mach-O read failed");
        }
        const auto info = anyios::macho::inspect(data);
        if (info.file_type != 2 || info.is_encrypted) {
            throw std::runtime_error("requires unencrypted arm64 MH_EXECUTE");
        }
        std::cout << "STATIC_IMPORTS=" << info.chained_imports.size() << "\n";
        if (info.legacy_dyld) {
            const auto& legacy = *info.legacy_dyld;
            std::cout << "LEGACY_DYLD_INFO=present\n"
                      << "LEGACY_REBASE_BYTES="
                      << (legacy.rebase ? legacy.rebase->file_size : 0) << "\n"
                      << "LEGACY_BIND_BYTES="
                      << (legacy.bind ? legacy.bind->file_size : 0) << "\n"
                      << "LEGACY_LAZY_BIND_BYTES="
                      << (legacy.lazy_bind ? legacy.lazy_bind->file_size : 0) << "\n"
                      << "LEGACY_WEAK_BIND_BYTES="
                      << (legacy.weak_bind ? legacy.weak_bind->file_size : 0) << "\n";
        }
        if (info.has_unixthread) {
            std::cout << "LEGACY_UNIXTHREAD=present-not-executable\n";
        }
        for (const auto& name : info.chained_imports) {
            std::cout << "MISSING_RUNTIME_IMPORT=" << name << "\n";
        }
        // These placeholders are never executable and never host addresses.
        // Success here can only mean sections/fixups staged, NOT app startup.
        std::vector<std::uint64_t> placeholders(
            info.chained_imports.size(), 0x900000);
        anyios::cpu::GuestMemory memory(0x10000, 16 * 1024 * 1024);
        const auto loaded = anyios::loader::stage_linked_image(
            data, memory, 0x10000, placeholders,
            anyios::loader::LinkedImageOptions{true, nullptr});
        std::cout << "STAGING=passed-metadata-only\n"
                  << "GUEST_ENTRY=" << loaded.guest_entry << "\n"
                  << "EXECUTION=refused-unimplemented-objc-uikit-runtime\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "STAGING=blocked\n"
                  << "FIRST_LOADER_BLOCKER=" << error.what() << "\n";
        return 3;
    }
}
