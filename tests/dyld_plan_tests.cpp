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

void test_discovery() {
    auto source = base_graph();
    source[1].image.rpaths.push_back("@executable_path");
    source[1].image.dependencies.push_back({"/System/Library/Frameworks/UIKit.framework/UIKit"});
    source[2].image.dependencies.push_back({"@rpath/Absent.dylib"});
    std::vector<std::string> requested;
    const auto reader = [&](std::string_view path) -> std::optional<anyios::macho::Image> {
        requested.emplace_back(path);
        for (const auto& item : source) if (item.path == path) return item.image;
        return std::nullopt;
    };
    auto graph = anyios::dyld::discover_dependencies("Payload/App.app/App", reader);
    require(graph.modules.size() == 3 && graph.modules.front().image.file_type == 2,
            "discovery lost root or reachable images");
    require(graph.plan.load_order == std::vector<std::string>({source[0].path, source[2].path, source[1].path}),
            "discovery dependency-first ordering wrong");
    require(graph.unresolved.size() == 3 && !graph.unresolved[0].weak &&
            graph.unresolved[0].install_name == "@rpath/Absent.dylib" &&
            graph.unresolved[1].weak && graph.unresolved[2].install_name.starts_with("/System/"),
            "missing bundle/system/weak requirements lost");
    for (const auto& path : requested) require(!path.starts_with('/'), "system dependency passed to host reader");
    source[1].image.dependencies.clear();
    require(graph.modules[0].image.dependencies.size() == 3, "discovery retained caller-owned metadata");

    auto simple = module("Payload/App.app/App", 2,
        {{"@rpath/Choice.dylib"}, {"@rpath/Choice.dylib"}, {"@rpath/Missing"}, {"@rpath/Missing"}},
        {"@executable_path/First", "@executable_path/Second"});
    unsigned first_reads = 0, second_reads = 0, absent_reads = 0;
    graph = anyios::dyld::discover_dependencies(simple.path,
        [&](std::string_view path) -> std::optional<anyios::macho::Image> {
            if (path == simple.path) return simple.image;
            if (path == "Payload/App.app/First/Choice.dylib") {
                ++first_reads; return module(std::string(path), 6).image;
            }
            if (path == "Payload/App.app/Second/Choice.dylib") ++second_reads;
            ++absent_reads; return std::nullopt;
        });
    require(first_reads == 1 && second_reads == 0 && absent_reads == 2 && graph.unresolved.size() == 2,
            "rpath precedence or missing-candidate cache wrong");
    simple.image.rpaths.insert(simple.image.rpaths.begin(), "/usr/lib/swift");
    requested.clear();
    graph = anyios::dyld::discover_dependencies(simple.path,
        [&](std::string_view path) -> std::optional<anyios::macho::Image> {
            requested.emplace_back(path);
            if (path == simple.path) return simple.image;
            if (path == "Payload/App.app/First/Choice.dylib") return module(std::string(path), 6).image;
            return std::nullopt;
        });
    require(graph.external_runpaths.size()==1 && graph.external_runpaths[0].loader==simple.path &&
            graph.external_runpaths[0].path=="/usr/lib/swift" && graph.modules.size()==2,
            "external guest runpath prevented real bundle metadata intake or lost prerequisite");
    for (const auto& path : requested) require(!path.starts_with('/'), "external runpath reached host reader");
    auto strict_modules = std::vector<Module>{simple, module("Payload/App.app/First/Choice.dylib",6)};
    fails(strict_modules,"unsupported dyld install-name prefix: /usr/lib/swift");
    auto rejects = [&](const anyios::dyld::ModuleReader& read, std::string_view reason) {
        bool rejected = false;
        try { (void)anyios::dyld::discover_dependencies(simple.path, read); }
        catch (const anyios::macho::FormatError& error) {
            rejected = std::string_view(error.what()).find(reason) != std::string_view::npos;
        }
        require(rejected, "invalid discovery accepted or diagnostic changed");
    };
    rejects({}, "missing dyld module reader");
    rejects([](std::string_view) -> std::optional<anyios::macho::Image> { return std::nullopt; }, "main dyld");
    simple.image.rpaths = {"/" + std::string(4096, 'x')};
    rejects([&](std::string_view) { return simple.image; }, "runpath length limit");
    simple.image.rpaths = {"/../outside"};
    rejects([&](std::string_view) { return simple.image; }, "escapes module registry");
    simple.image.rpaths = {std::string("/usr/\0lib",9)};
    rejects([&](std::string_view) { return simple.image; }, "invalid bundle-relative path");
    simple.image.rpaths = {"@unsupported/value"};
    rejects([&](std::string_view) { return simple.image; }, "unsupported dyld install-name prefix: @unsupported/value");
    simple.image.rpaths.assign(4097,"/usr/lib/swift");
    rejects([&](std::string_view) { return simple.image; }, "external runpath limit");
    simple.image.rpaths.assign(513, "@executable_path");
    rejects([&](std::string_view) { return simple.image; }, "runpath limit");
    simple.image.rpaths = {"@executable_path"};
    rejects([&](std::string_view) { return simple.image; }, "not MH_DYLIB");
    simple.image.dependencies = {{"@loader_path/../../../outside"}};
    rejects([&](std::string_view) { return simple.image; }, "escapes module registry");
    simple.image.dependencies = {{"@executable_path/App"}};
    rejects([&](std::string_view) { return simple.image; }, "circular dyld");
    simple.image.dependencies = {{std::string(4097, 'x')}};
    rejects([&](std::string_view) { return simple.image; }, "invalid dyld dependency");
    simple.image.dependencies = {{std::string("@rpath/a\0b", 10)}};
    rejects([&](std::string_view) { return simple.image; }, "invalid dyld dependency");
    bool propagated = false;
    try {
        (void)anyios::dyld::discover_dependencies(simple.path,
            [](std::string_view) -> std::optional<anyios::macho::Image> { throw std::runtime_error("malformed-file"); });
    } catch (const std::runtime_error& error) { propagated = std::string_view(error.what()) == "malformed-file"; }
    require(propagated, "reader failure converted to absent module");
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
        test_discovery();
        test_macho_dependency_metadata();
        std::cout << "dyld module graph and Mach-O load command tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
