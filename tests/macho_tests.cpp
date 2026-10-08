#include <anyios/macho.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
namespace {
using Bytes = std::vector<std::byte>;
using anyios::macho::FormatError;
void u32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    if (bytes.size() < at + 4) bytes.resize(at + 4);
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = std::byte((value >> (i * 8)) & 0xff);
}
void u64(Bytes& bytes, std::size_t at, std::uint64_t value) {
    u32(bytes, at, static_cast<std::uint32_t>(value));
    u32(bytes, at + 4, static_cast<std::uint32_t>(value >> 32));
}
Bytes command(std::uint32_t type, std::size_t size) {
    Bytes bytes(size);
    u32(bytes, 0, type);
    u32(bytes, 4, static_cast<std::uint32_t>(size));
    return bytes;
}
Bytes image(const std::vector<Bytes>& commands) {
    std::size_t region_size = 0;
    for (const auto& command : commands) region_size += command.size();
    Bytes bytes(32 + region_size);
    u32(bytes, 0, 0xfeedfacf);
    u32(bytes, 4, 0x0100000c);
    u32(bytes, 8, 0);
    u32(bytes, 12, 2);
    u32(bytes, 16, static_cast<std::uint32_t>(commands.size()));
    u32(bytes, 20, static_cast<std::uint32_t>(region_size));
    std::size_t at = 32;
    for (const auto& data : commands) {
        std::copy(data.begin(), data.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at));
        at += data.size();
    }
    return bytes;
}
Bytes segment() {
    auto data = command(0x19, 72);
    const std::string name = "__TEXT";
    for (std::size_t i = 0; i < name.size(); ++i) data[8 + i] = std::byte(name[i]);
    u64(data, 24, 0x100000000ULL);
    u64(data, 32, 0x1000);
    u64(data, 40, 0);
    u64(data, 48, 0);
    return data;
}
Bytes dylib(std::string_view name) {
    const auto size = (24 + name.size() + 1 + 7) & ~std::size_t(7);
    auto data = command(0xc, size);
    u32(data, 8, 24);
    for (std::size_t i = 0; i < name.size(); ++i) data[24 + i] = std::byte(name[i]);
    return data;
}
void require(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}
void invalid(const Bytes& data, std::string_view message) {
    try {
        anyios::macho::inspect(data);
    } catch (const FormatError& error) {
        require(std::string_view(error.what()).find(message) != std::string_view::npos,
                std::string("expected [") + std::string(message) + "] got [" + error.what() + "]");
        return;
    }
    throw std::runtime_error("invalid input was accepted");
}
void test_valid() {
    auto main = command(0x80000028, 24);
    u64(main, 8, 2048);
    auto version = command(0x32, 24);
    u32(version, 8, 2);
    u32(version, 12, 0x000c0301);
    u32(version, 16, 0x00120000);
    auto runpath = command(0x8000001c, 32);
    u32(runpath, 8, 12);
    const std::string value = "@loader_path";
    for (std::size_t i = 0; i < value.size(); ++i) runpath[12 + i] = std::byte(value[i]);
    const auto result = anyios::macho::inspect(image({segment(), dylib("UIKit"), main, version, runpath}));
    require(result.file_type == 2 && result.command_count == 5, "header parsed incorrectly");
    require(result.segments.size() == 1 && result.segments[0].name == "__TEXT", "segment incorrect");
    require(result.segments[0].vm_address == 0x100000000ULL, "64-bit address lost");
    require(result.libraries.size() == 1 && result.libraries[0] == "UIKit", "dylib incorrect");
    require(result.has_entry && result.entry_offset == 2048, "entry offset incorrect");
    require(result.versions.size() == 1 && result.versions[0].platform == 2, "version incorrect");
    require(result.rpaths.size() == 1 && result.rpaths[0] == "@loader_path", "rpath incorrect");
    require(anyios::macho::version_string(0x000c0301) == "12.3.1", "version string incorrect");
}
void test_failures() {
    invalid({}, "magic");
    invalid(Bytes{std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'}}, "unsupported format");
    invalid(Bytes{std::byte{0xca}, std::byte{0xfe}, std::byte{0xba}, std::byte{0xbe}}, "truncated universal header");
    invalid(Bytes{std::byte{0xcf}, std::byte{0xfa}, std::byte{0xed}, std::byte{0xfe}}, "header");
    auto data = image({});
    u32(data, 4, 0x01000007);
    invalid(data, "unsupported CPU");
    data = image({});
    u32(data, 20, 0xffffffff);
    invalid(data, "load command region");
    data = image({});
    u32(data, 16, 1);
    invalid(data, "count exceeds");
    data = image({command(0x19, 8)});
    invalid(data, "truncated LC_SEGMENT_64");
    data = image({command(0x19, 80)});
    u32(data, 32 + 64, 1);
    invalid(data, "sections exceed");
    auto broken_segment = segment();
    u64(broken_segment, 40, 0xffffffffffffffffULL);
    u64(broken_segment, 48, 8);
    invalid(image({broken_segment}), "segment file range");
    broken_segment = segment();
    u64(broken_segment, 32, 2);
    u64(broken_segment, 48, 3);
    invalid(image({broken_segment}), "virtual memory range");
    broken_segment = segment();
    u64(broken_segment, 24, 0xffffffffffffffffULL);
    invalid(image({broken_segment}), "virtual memory range");
    data = image({command(0xc, 16)});
    invalid(data, "truncated dylib");
    auto bad_dylib = command(0xc, 32);
    u32(bad_dylib, 8, 33);
    invalid(image({bad_dylib}), "dylib name offset");
    u32(bad_dylib, 8, 24);
    std::fill(bad_dylib.begin() + 24, bad_dylib.end(), std::byte{'x'});
    invalid(image({bad_dylib}), "NUL-terminated");
    data = image({command(0x8000001c, 8)});
    invalid(data, "truncated LC_RPATH");
    data = image({command(0x80000028, 16)});
    invalid(data, "truncated LC_MAIN");
    data = image({command(0x80000028, 24), command(0x80000028, 24)});
    invalid(data, "duplicate LC_MAIN");
    data = image({command(0x32, 16)});
    invalid(data, "truncated LC_BUILD_VERSION");
    auto build = command(0x32, 24);
    u32(build, 20, 1);
    invalid(image({build}), "tools exceed");
    data = image({command(0x25, 8)});
    invalid(data, "truncated LC_VERSION_MIN_IPHONEOS");
    data = image({command(0x2c, 16)});
    invalid(data, "truncated LC_ENCRYPTION_INFO_64");
    auto encrypt = command(0x2c, 24);
    u32(encrypt, 8, 0xffffffff);
    u32(encrypt, 12, 2);
    invalid(image({encrypt}), "encrypted file range");
    data = image({command(0x19, 72)});
    u32(data, 32 + 4, 0);
    invalid(data, "load command size");
    data = image({command(0x19, 72)});
    u32(data, 32 + 4, 71);
    invalid(data, "load command size");
    data = image({command(0x19, 72)});
    u32(data, 32 + 4, 80);
    invalid(data, "load command size");
    data = image({command(0x19, 72)});
    u32(data, 16, 0);
    invalid(data, "does not consume");
}
void test_encrypted() {
    auto command_data = command(0x2c, 24);
    u32(command_data, 16, 1);
    require(anyios::macho::inspect(image({command_data})).is_encrypted, "encrypted image not flagged");
    u32(command_data, 16, 0);
    require(!anyios::macho::inspect(image({command_data})).is_encrypted, "clear image incorrectly flagged");
}
void test_legacy_version() {
    auto command_data = command(0x25, 16);
    u32(command_data, 8, 0x00080000);
    u32(command_data, 12, 0x00090000);
    const auto result = anyios::macho::inspect(image({command_data}));
    require(result.versions.size() == 1 && result.versions[0].minimum_os == 0x00080000,
            "legacy deployment target missing");
}
void test_deterministic_mutation() {
    const auto original = image({segment(), dylib("UIKit"), command(0x32, 24)});
    std::uint32_t seed = 0x12345678;
    for (unsigned sample = 0; sample < 3000; ++sample) {
        auto data = original;
        seed = seed * 1664525u + 1013904223u;
        const auto index = static_cast<std::size_t>(seed) % (data.size() + 1);
        if (index == data.size()) data.resize(static_cast<std::size_t>(seed & 63u));
        else data[index] = std::byte((seed >> 16) & 0xff);
        try {
            (void)anyios::macho::inspect(data);
        } catch (const FormatError&) {
        }
    }
}
}
int main() {
    try {
        test_valid();
        test_failures();
        test_encrypted();
        test_legacy_version();
        test_deterministic_mutation();
        std::cout << "Mach-O parser tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Mach-O parser tests failed: " << error.what() << "\n";
        return 1;
    }
}
