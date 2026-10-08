#include <anyios/linked_pair.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<std::byte> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error(std::string("cannot open: ") + path);
    const auto n = stream.tellg();
    if (n <= 0 || n > 32 * 1024 * 1024) {
        throw std::runtime_error("invalid owned Mach-O fixture length");
    }
    stream.seekg(0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(n));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), n)) {
        throw std::runtime_error("owned Mach-O fixture read failed");
    }
    return bytes;
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: anyios-link-probe owned-RuntimeApp owned-libRuntimeWidget.dylib\n";
        return 2;
    }
    try {
        auto executable = read_file(argv[1]);
        auto library = read_file(argv[2]);
        anyios::cpu::GuestMemory memory(0x10000, 4 * 1024 * 1024);
        const auto result = anyios::loader::stage_owned_linked_pair(
            executable, library, memory, 0x10000, 0x80000);
        std::cout << "Owned linked iPhoneOS images staged"
                  << " entry=" << result.entry
                  << " imported_function=" << result.imported_function
                  << " executable_fixups=" << result.executable_fixups
                  << " library_fixups=" << result.library_fixups << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Owned linked iPhoneOS images unsupported: " << e.what() << '\n';
        return 1;
    }
}
