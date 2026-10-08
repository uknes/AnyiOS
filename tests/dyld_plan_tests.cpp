#include <anyios/dyld_plan.hpp>
#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using anyios::dyld::Module;
using anyios::macho::Dependency;

void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

void fails(const std::vector<Module>& modules, std::string_view expected) {
    try {
        static_cast<void>(anyios::dyld::plan_dependencies(modules, "Payload/App.app/App"));
    } catch (const anyios::macho::FormatError& error) {
        require(std::string_view(error.what()).find(expected) != std::string_view::npos,
                std::string("unexpected error: ") + error.what());
        return;
    }
    throw std::runtime_error("invalid dyld graph accepted");
}

Module module(std::string path, std::uint32_t file_type,
              std::vector<Dependency> dependencies = {},
              std::vector<std::string> rpaths = {}) {
    anyios::macho::Image image;
    image.file_type = file_type;
    image.dependencies = std::move(dependencies);
    image.rpaths = std::move(rpaths);
    return {std::move(path), std::move(image)};
}

std::vector<Module> base_graph() {
    return {
        module("Payload/App.app/Frameworks/Bar.dylib", 6),
        module("Payload/App.app/App", 2,
               {{"@rpath/Foo.framework/Foo"}, {"@rpath/Optional.dylib", true}},
               {"@executable_path/Frameworks"}),
        module("Payload/App.app/Frameworks/Foo.framework/Foo", 6,
               {{"@loader_path/../Bar.dylib"}})
    };
}

void test_graph() {
    auto modules = base_graph();
    const auto plan = anyios::dyld::plan_dependencies(modules, "Payload/App.app/App");
    require(plan.load_order == std::vector<std::string>({
                "Payload/App.app/Frameworks/Bar.dylib",
                "Payload/App.app/Frameworks/Foo.framework/Foo",
                "Payload/App.app/App"}), "incorrect dependency-first order");
    require(plan.missing_weak.size() == 1 &&
            plan.missing_weak[0].install_name == "@rpath/Optional.dylib", "missing weak dependency");

    modules = base_graph();
    modules[1].image.dependencies.push_back({"@rpath/Missing.dylib"});
    fails(modules, "unresolved dyld dependency");
    modules = base_graph();
    modules[2].image.dependencies.push_back({"@executable_path/App"});
    fails(modules, "circular dyld dependency");
    modules = base_graph();
    modules[0].path = modules[2].path;
    fails(modules, "duplicate or noncanonical");
    modules = base_graph();
    modules[0].path = "Payload/App.app/Frameworks/../Bar.dylib";
    fails(modules, "duplicate or noncanonical");
    modules = base_graph();
    modules[2].image.dependencies.push_back({"@loader_path/../../../../../../Windows/system32"});
    fails(modules, "escapes module registry");
    modules = base_graph();
    modules[2].image.dependencies.push_back({"/usr/lib/libSystem.B.dylib"});
    fails(modules, "system or bare dylib");
    modules = base_graph();
    modules[2].image.file_type = 2;
    fails(modules, "not MH_DYLIB");
    modules = base_graph();
    modules[1].image.rpaths.push_back("@rpath/Nested");
    fails(modules, "nested @rpath");
}

void w32(std::vector<std::byte>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(at + i) = std::byte((value >> (8 * i)) & 255);
}

void append_command(std::vector<std::byte>& bytes, std::uint32_t kind, std::string_view name) {
    const auto size = (24 + name.size() + 1 + 7) & ~std::size_t(7);
    const auto offset = bytes.size();
    bytes.resize(offset + size);
    w32(bytes, offset, kind);
    w32(bytes, offset + 4, static_cast<std::uint32_t>(size));
    w32(bytes, offset + 8, 24);
    for (std::size_t i = 0; i < name.size(); ++i) {
        bytes[offset + 24 + i] = std::byte(name[i]);
    }
}

void test_macho_dependency_metadata() {
    std::vector<std::byte> bytes(32);
    w32(bytes, 0, 0xfeedfacf);
    w32(bytes, 4, 0x0100000c);
    w32(bytes, 12, 6);
    append_command(bytes, 0x0d, "@rpath/Foo.dylib");
    append_command(bytes, 0x80000018, "@rpath/Optional.dylib");
    w32(bytes, 16, 2);
    w32(bytes, 20, static_cast<std::uint32_t>(bytes.size() - 32));
    const auto image = anyios::macho::inspect(bytes);
    require(image.install_name == "@rpath/Foo.dylib", "LC_ID_DYLIB parsed incorrectly");
    require(image.dependencies.size() == 1 && image.dependencies[0].weak &&
            image.dependencies[0].install_name == "@rpath/Optional.dylib",
            "weak dependency metadata");
    require(image.libraries.size() == 1, "legacy dependency metadata lost");

    append_command(bytes, 0x0d, "@rpath/Other.dylib");
    w32(bytes, 16, 3);
    w32(bytes, 20, static_cast<std::uint32_t>(bytes.size() - 32));
    try {
        static_cast<void>(anyios::macho::inspect(bytes));
        throw std::runtime_error("duplicate dylib ID accepted");
    } catch (const anyios::macho::FormatError& error) {
        require(std::string_view(error.what()).find("duplicate LC_ID_DYLIB") !=
                std::string_view::npos, "duplicate ID diagnostic");
    }
}
}

int main() {
    try {
        test_graph();
        test_macho_dependency_metadata();
        std::cout << "dyld module graph and Mach-O load command tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
