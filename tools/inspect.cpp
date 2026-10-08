#include <anyios/macho.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: anyios-inspect <unprotected-arm64-macho-file>\n";
        return 0;
    }
    if (argc != 2) {
        std::cerr << "Usage: anyios-inspect <unprotected-arm64-macho-file>\n";
        return 1;
    }
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Cannot open input file\n";
        return 1;
    }
    const auto length = file.tellg();
    constexpr std::streamoff maximum_size = 1024LL * 1024 * 1024;
    if (length < 0 || length > maximum_size) {
        std::cerr << "Input file exceeds the 1 GiB inspection limit\n";
        return 1;
    }
    file.seekg(0);
    std::vector<std::byte> data(static_cast<std::size_t>(length));
    if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        std::cerr << "Failed to read input file\n";
        return 1;
    }
    try {
        const auto result = anyios::macho::inspect(data);
        std::cout << "Format: Mach-O 64-bit ARM64\n"
                  << "File type: " << result.file_type << "\n"
                  << "CPU subtype: " << result.cpu_subtype << "\n"
                  << "Load commands: " << result.command_count << "\n"
                  << "Encrypted code: " << (result.is_encrypted ? "yes (not supported)" : "no") << "\n";
        if (result.is_fat) std::cout << "Container: universal (" << result.architecture_count << " architectures), selected offset=" << result.slice_offset << " size=" << result.slice_size << "\n";
        if (result.has_entry) std::cout << "Entry file offset: " << result.entry_offset << "\n";
        for (const auto& version : result.versions) {
            std::cout << "Platform " << version.platform << ": min "
                      << anyios::macho::version_string(version.minimum_os) << ", SDK "
                      << anyios::macho::version_string(version.sdk) << "\n";
        }
        for (const auto& segment : result.segments) {
            std::cout << "Segment: " << segment.name << " file=" << segment.file_offset
                      << "+" << segment.file_size << " vm=" << segment.vm_address
                      << "+" << segment.vm_size << " sections=" << segment.sections << "\n";
        }
        if (result.has_chained_fixups) std::cout << "Chained imports: " << result.chained_imports.size() << "\n";
        if (result.has_export_trie) std::cout << "Exported symbols: " << result.exported_symbols.size() << "\n";
        for (const auto& symbol : result.chained_imports) std::cout << "Import: " << symbol << "\n";
        for (const auto& symbol : result.exported_symbols) std::cout << "Export: " << symbol << "\n";
        for (const auto& library : result.libraries) std::cout << "Dylib: " << library << "\n";
        for (const auto& rpath : result.rpaths) std::cout << "Rpath: " << rpath << "\n";
        return result.is_encrypted ? 3 : 0;
    } catch (const anyios::macho::FormatError& error) {
        std::cerr << "Invalid or unsupported Mach-O: " << error.what() << "\n";
        return 2;
    }
}
