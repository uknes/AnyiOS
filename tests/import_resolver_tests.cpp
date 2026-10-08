#include <anyios/import_resolver.hpp>
#include <anyios/fixup_plan.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using anyios::macho::Image;
using anyios::dyld::LoadedDylib;
using Bytes = std::vector<std::byte>;

void word(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        bytes.at(at + i) = std::byte((value >> (i * 8)) & 0xff);
    }
}
void check(bool good, std::string_view message) {
    if (!good) throw std::runtime_error(std::string(message));
}
void reject(const Bytes& payload, const Image& image, const Image& lib,
            std::string_view expected) {
    const std::array<LoadedDylib, 1> libraries{{{&lib, 0x10000}}};
    try {
        (void)anyios::dyld::resolve_chained_import_targets(payload, image, libraries);
    } catch (const anyios::macho::FormatError& error) {
        check(std::string_view(error.what()).find(expected) != std::string_view::npos,
              std::string("wrong rejection: ") + error.what());
        return;
    }
    throw std::runtime_error("invalid chained import accepted");
}

void run() {
    Bytes payload(32);
    word(payload, 8, 28);
    word(payload, 16, 1);
    word(payload, 20, 1);
    word(payload, 28, 1);
    Image importer;
    importer.file_type = 2;
    importer.chained_fixups_range = anyios::macho::LinkeditRange{0, 32};
    importer.chained_imports = {"_anyios_widget"};
    importer.dependencies = {{"@rpath/libWidget.dylib"}};

    Image lib;
    lib.file_type = 6;
    lib.install_name = "@rpath/libWidget.dylib";
    lib.symbols = {{"_anyios_widget", 0x1000, 0x0f, 1}};
    std::array<LoadedDylib, 1> libraries{{{&lib, 0x10000}}};

    const auto addresses = anyios::dyld::resolve_chained_import_targets(payload, importer, libraries);
    check(addresses.size() == 1 && addresses[0] == 0x11000, "two-level imported symbol resolution");

    lib.symbols = {{"_anyios_widget", 0x1000, 0x03, 0}};
    check(anyios::dyld::resolve_chained_import_targets(payload, importer, libraries)[0] == 0x1000,
          "absolute exported symbol incorrectly slid");
    lib.symbols = {{"_anyios_widget", 0x1000, 0x0f, 1},
                   {"_anyios_widget", 0x2000, 0x1f, 1}};
    check(anyios::dyld::resolve_chained_import_targets(payload, importer, libraries)[0] == 0x11000,
          "private export incorrectly shadowed public export");
    lib.symbols.push_back({"_anyios_widget", 0x3000, 0x0f, 1});
    reject(payload, importer, lib, "ambiguous chained exported symbol");

    lib.symbols = {{"_anyios_widget", 0x1000, 0x0e, 1}};
    reject(payload, importer, lib, "unresolved chained exported symbol");
    lib.symbols = {{"_anyios_widget", 0x1000, 0x01, 0}};
    reject(payload, importer, lib, "unresolved chained exported symbol");
    lib.symbols.clear();
    lib.exports = {{"_anyios_widget", 0x300, 0}};
    lib.segments = {{"__TEXT", 0x1000, 4096, 0, 4096, 0}};
    check(anyios::dyld::resolve_chained_import_targets(payload, importer, libraries)[0] ==
          0x11300, "dyld export trie offset resolution");
    lib.exports[0].flags = 2;
    check(anyios::dyld::resolve_chained_import_targets(payload, importer, libraries)[0] ==
          0x300, "absolute trie export incorrectly slid");
    lib.exports[0].flags = 8;
    reject(payload, importer, lib, "unsupported reexport");
    lib.exports[0].flags = 1;
    reject(payload, importer, lib, "unsupported reexport");
    lib.exports.clear();
    lib.symbols = {{"_anyios_widget", 0x1000, 0x0f, 1}};
    word(payload, 28, 0);
    reject(payload, importer, lib, "unsupported chained import library ordinal");
    word(payload, 28, 2);
    reject(payload, importer, lib, "unsupported chained import library ordinal");
    word(payload, 28, 0x101);
    lib.symbols.clear();
    reject(payload, importer, lib, "unresolved weak chained bind unsupported");
    lib.symbols = {{"_anyios_widget", 0x1000, 0x0f, 1}};
    word(payload, 28, 1);
    lib.install_name = "@rpath/Other.dylib";
    reject(payload, importer, lib, "chained import dependency not loaded");
    lib.install_name = "@rpath/libWidget.dylib";
    importer.chained_imports = {"_anyios_widget", "_second"};
    reject(payload, importer, lib, "chained import names/count mismatch");
    importer.chained_imports = {"_anyios_widget"};
    word(payload, 20, 3);
    reject(payload, importer, lib, "unsupported chained import addend format");
    word(payload, 20, 1);

    auto truncated = payload;
    truncated.resize(29);
    reject(truncated, importer, lib, "chained import payload out of bounds");
    importer.chained_fixups_range = anyios::macho::LinkeditRange{0, 29};
    reject(truncated, importer, lib, "chained import table out of bounds");
}
}

int main() {
    try {
        run();
        std::cout << "Chained two-level import target resolution checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "chained import resolution tests failed: " << error.what() << '\n';
        return 1;
    }
}
