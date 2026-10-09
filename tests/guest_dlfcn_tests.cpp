#include <anyios/guest_dlfcn.hpp>

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using anyios::cpu::Access;
using anyios::cpu::GuestMemory;
using anyios::dyld::GuestModuleRegistry;
void check(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
template<class F> void refused(F operation) {
    bool failed = false;
    try { operation(); } catch (const anyios::macho::FormatError&) { failed = true; }
    check(failed, "unsupported/malformed lookup was accepted");
}
anyios::macho::Image image(bool main, const char* name, std::uint64_t preferred = 0x100000000) {
    anyios::macho::Image result;
    result.file_type = main ? 2 : 6;
    result.install_name = name;
    result.has_export_trie = true;
    result.segments.push_back({"__TEXT", preferred, 0x4000, 0, 0x4000, 0});
    result.exports.push_back({"_value", 0x100, 0});
    return result;
}
void string(GuestMemory& memory, std::uint64_t address, const std::string& value) {
    check(memory.copy_to(address, std::as_bytes(std::span(value.c_str(), value.size() + 1))), "name store failed");
}
void run() {
    GuestMemory memory(0x10000, 0x100000);
    const auto rx = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::execute);
    const auto rw = anyios::cpu::bits(Access::read) | anyios::cpu::bits(Access::write);
    check(memory.map_ios(0x10000, 0x4000, rx) && memory.map_ios(0x20000, 0x4000, rx) &&
          memory.map_ios(0x30000, 0x4000, rw), "setup mapping failed");
    auto main = image(true, "main");
    main.segments.push_back({"__DATA", 0x100020000, 0x4000, 0, 0x4000, 0});
    main.exports.push_back({"_data", 0x20000, 0});
    main.dependencies.push_back({"@rpath/libOwned.dylib"});
    auto library = image(false, "@rpath/libOwned.dylib", 0);
    library.exports.push_back({"_library_only", 0x200, 4});
    library.exports.push_back({"_tls", 0, 1});
    library.exports.push_back({"_absolute", 0, 2});
    library.exports.push_back({"_reexport", 0, 8});
    library.exports.push_back({"_resolver", 0, 16});
    GuestModuleRegistry modules(memory);
    modules.add_image(main, 0x10000, true);
    check(modules.lookup(anyios::dyld::rtld_main_only, "value") == 0x10100, "negative slide/main export failed");
    refused([&] { (void)modules.lookup(anyios::dyld::rtld_default, "value"); });
    refused([&] { modules.seal(); });
    check(modules.size() == 1, "failed seal mutated registration");
    modules.add_image(library, 0x20000);
    modules.seal();
    check(modules.lookup(anyios::dyld::rtld_default, "value") == 0x10100, "global load order failed");
    check(modules.lookup(anyios::dyld::rtld_main_only, "data") == 0x30000 && !memory.fetch(0x30000),
          "readable non-executable guest data export failed");
    check(modules.lookup(anyios::dyld::rtld_default, "library_only") == 0x20200, "library export failed");
    check(!modules.lookup(anyios::dyld::rtld_main_only, "library_only"), "main-only scope leaked library");
    check(!modules.lookup(anyios::dyld::rtld_default, "_value"), "C name prefix was stripped incorrectly");
    for (const char* name : {"tls", "absolute", "reexport", "resolver"})
        refused([&] { (void)modules.lookup(anyios::dyld::rtld_default, name); });
    for (auto handle : {UINT64_MAX, UINT64_MAX - 2, std::uint64_t{0}, std::uint64_t{0x20000}})
        refused([&] { (void)modules.lookup(handle, "value"); });
    refused([&] { modules.add_image(library, 0x20000); });
    anyios::dyld::GuestDlState errors(memory, modules, 0x40000, 2);
    string(memory, 0x30000, "missing");
    check(errors.dlsym(1, anyios::dyld::rtld_default, 0x30000) == 0, "missing lookup did not return NULL");
    check(errors.dlerror(2) == 0, "guest thread error leaked");
    const auto first = errors.dlerror(1);
    check(first == 0x40000 && memory.read(first, 1) == 'd' && errors.dlerror(1) == 0, "error pointer/reset failed");
    check(memory.read(first, 1) == 'd', "dlerror destroyed returned string before reuse");
    check(!memory.fetch(first), "error page became executable");
    check(errors.dlsym(2, anyios::dyld::rtld_default, 0x30000) == 0, "second thread missing failed");
    check(errors.dlsym(1, anyios::dyld::rtld_default, 0x30000) == 0, "reset error failed");
    refused([&] { (void)errors.dlsym(1, UINT64_MAX, 0x30000); });
    string(memory, 0x30000, "value");
    check(errors.dlsym(1, anyios::dyld::rtld_default, 0x30000) == 0x10100 && errors.dlerror(1) == 0,
          "success did not clear calling guest thread error");
    check(errors.dlerror(2) == 0x44000, "success cleared another guest thread's error");
    refused([&] { (void)errors.dlerror(3); });
    refused([&] { (void)errors.dlsym(1, anyios::dyld::rtld_default, UINT64_MAX); });
    refused([&] { (void)errors.dlsym(1, anyios::dyld::rtld_default, 0x34000); });
    string(memory, 0x30000, std::string(1024, 'x'));
    check(errors.dlsym(1, anyios::dyld::rtld_default, 0x30000) == 0, "1024-byte name was refused");
    string(memory, 0x30000, std::string(1025, 'x'));
    refused([&] { (void)errors.dlsym(1, anyios::dyld::rtld_default, 0x30000); });
    check(errors.dlerror(1) == first, "refusal consumed previous guest error");
    check(memory.write(0x33fff, 'x', 1), "boundary write failed");
    refused([&] { (void)errors.dlsym(1, anyios::dyld::rtld_default, 0x33fff); });
    refused([&] { anyios::dyld::GuestDlState bad(memory, modules, 1); });
    refused([&] { anyios::dyld::GuestDlState bad(memory, modules, 0x40000, 0); });
    refused([&] { anyios::dyld::GuestDlState bad(memory, modules, UINT64_MAX - 0x3fff, 2); });
    anyios::dyld::GuestDlState occupied(memory, modules, 0x30000);
    refused([&] { (void)occupied.dlerror(1); });
    check(memory.read(0x33fff, 1) == 'x', "occupied error mapping was changed");

    GuestModuleRegistry malformed(memory);
    auto duplicate = main;
    duplicate.exports.push_back(duplicate.exports.front());
    refused([&] { malformed.add_image(duplicate, 0x10000, true); });
    check(malformed.size() == 0, "failed registration partially committed");
    auto bad = main; bad.exports[0].address = 0x4000;
    refused([&] { malformed.add_image(bad, 0x10000, true); });
    bad = main; bad.exports[0].address = UINT64_MAX;
    refused([&] { malformed.add_image(bad, 0x10000, true); });
    bad = main; bad.has_export_trie = false;
    refused([&] { malformed.add_image(bad, 0x10000, true); });
    bad = main; bad.exports[0].name = std::string("_bad\0name", 9);
    refused([&] { malformed.add_image(bad, 0x10000, true); });
    malformed.add_image(main, 0x10000, true);
    refused([&] { malformed.add_image(main, 0x10000, true); });
    const std::array<anyios::dyld::RuntimeExport, 1> host_pointer{{{"_unsafe", UINT64_MAX}}};
    refused([&] { malformed.add_runtime_module("runtime", host_pointer); });
    const std::array<anyios::dyld::RuntimeExport, 1> data_pointer{{{"_unsafe", 0x30000}}};
    refused([&] { malformed.add_runtime_module("runtime", data_pointer); });
    check(malformed.size() == 1, "invalid runtime module was committed");
    GuestModuleRegistry local(memory);
    auto root = image(true, "main");
    root.dependencies.push_back({"absent-weak", true});
    local.add_image(root, 0x10000, true);
    local.add_image(library, 0x20000, false, false);
    local.seal();
    check(!local.lookup(anyios::dyld::rtld_default, "library_only"), "RTLD_LOCAL export leaked global scope");

    GuestModuleRegistry lifetime(memory);
    auto temporary = image(false, "temporary");
    lifetime.add_image(root, 0x10000, true);
    {
        GuestMemory::MappingJournal journal(memory);
        check(journal.map(0x60000, 0x4000, rx, true), "temporary mapping failed");
        temporary.exports[0].name = "_temporary";
        lifetime.add_image(temporary, 0x60000);
    }
    lifetime.seal();
    refused([&] { (void)lifetime.lookup(anyios::dyld::rtld_default, "temporary"); });
    auto fallback = image(true, "main");
    fallback.has_export_trie = false;
    fallback.has_symbol_table = true;
    fallback.exports.clear();
    fallback.sections.push_back({"__text", "__TEXT", 0x100000000, 0x4000, 0, 0, false});
    fallback.symbols = {
        {"_table", 0x100000100, 0x0f, 1}, {"_private", 0x100000100, 0x1f, 1},
        {"_local", 0x100000100, 0x0e, 1}, {"_undefined", 0, 1, 0},
        {"_debug", 0x100000100, 0xef, 1}, {"_absolute", 123, 3, 0}
    };
    GuestModuleRegistry symbols(memory);
    symbols.add_image(fallback, 0x10000, true);
    symbols.seal();
    fallback.symbols[0].value = UINT64_MAX;
    check(symbols.lookup(anyios::dyld::rtld_default, "table") == 0x10100,
          "nlist resolution or metadata snapshot ownership failed");
    for (const char* hidden : {"private", "local", "undefined", "debug"})
        check(!symbols.lookup(anyios::dyld::rtld_default, hidden), "non-exported nlist symbol leaked");
    refused([&] { (void)symbols.lookup(anyios::dyld::rtld_default, "absolute"); });
    fallback.symbols[0].value = 0x100000100;
    fallback.symbols[0].section_index = 2;
    GuestModuleRegistry invalid_section(memory);
    refused([&] { invalid_section.add_image(fallback, 0x10000, true); });
    check(invalid_section.size() == 0, "invalid nlist section partially registered");
    refused([&] { (void)errors.dlsym(1, anyios::dyld::rtld_default, 0); });
    GuestModuleRegistry budget(memory);
    auto bounded = image(true, "main");
    budget.add_image(bounded, 0x10000, true);
    bounded.file_type = 6;
    for (unsigned n = 1; n < 256; ++n) {
        bounded.install_name = "module-" + std::to_string(n);
        budget.add_image(bounded, 0x20000);
    }
    bounded.install_name = "one-too-many";
    refused([&] { budget.add_image(bounded, 0x20000); });
    check(budget.size() == 256, "module limit partially committed an image");
    GuestModuleRegistry strings(memory);
    auto excessive = image(true, "main");
    excessive.exports.resize(65537);
    refused([&] { strings.add_image(excessive, 0x10000, true); });
    excessive = image(true, "main");
    excessive.exports.clear();
    for (unsigned n = 0; n < 2048; ++n) {
        auto label = std::to_string(n);
        label.resize(4096, 'x');
        excessive.exports.push_back({label, 0x100, 0});
    }
    refused([&] { strings.add_image(excessive, 0x10000, true); });
    check(strings.size() == 0, "name budget partially committed an image");
}
}
int main() {
    try { run(); std::cout << "Guest dynamic lookup scope, ownership and thread errors passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
