#include <anyios/guest_dlfcn.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace anyios::dyld {
namespace {
constexpr std::size_t max_modules = 256;
constexpr std::size_t max_exports = 65536;
constexpr std::size_t max_name = 1024;
constexpr std::size_t max_names = 8 * 1024 * 1024;

std::uint64_t add(std::uint64_t a, std::uint64_t b) {
    if (b > UINT64_MAX - a) throw macho::FormatError("guest export address overflow");
    return a + b;
}
void validate_name(std::string_view name) {
    if (name.empty() || name.size() > 4096 || name.find('\0') != name.npos)
        throw macho::FormatError("invalid guest module/export name");
}
void count_name(std::size_t& total, std::string_view name) {
    validate_name(name);
    if (name.size() > max_names - total)
        throw macho::FormatError("guest module names exceed safety limit");
    total += name.size();
}
}

void GuestModuleRegistry::append(Module module) {
    if (sealed_) throw macho::FormatError("guest module registry is sealed");
    if (modules_.size() == max_modules || module.exports.size() > max_exports - export_count_)
        throw macho::FormatError("guest module/export limit exceeded");
    std::size_t names = name_bytes_;
    count_name(names, module.name);
    for (const auto& dependency : module.dependencies) count_name(names, dependency.install_name);
    for (const auto& symbol : module.exports) count_name(names, symbol.first);
    for (const auto& existing : modules_) {
        if (existing.name == module.name || (existing.main && module.main))
            throw macho::FormatError("duplicate guest module or main image");
    }
    if (module.main && !modules_.empty())
        throw macho::FormatError("main guest image must be first in load order");
    const auto count = module.exports.size();
    modules_.push_back(std::move(module));
    export_count_ += count;
    name_bytes_ = names;
}

void GuestModuleRegistry::add_image(const macho::Image& image,
                                     std::uint64_t guest_text_base,
                                     bool main, bool global) {
    if (image.is_fat || image.is_encrypted || image.file_type != (main ? 2U : 6U))
        throw macho::FormatError("guest symbol registry requires mapped thin ARM64 images");
    if (main && !global) throw macho::FormatError("main executable must have global lookup scope");
    if (!image.has_export_trie && !image.has_symbol_table)
        throw macho::FormatError("guest image has no supported export metadata");
    const auto text = std::find_if(image.segments.begin(), image.segments.end(),
        [](const macho::Segment& segment) { return segment.name == "__TEXT"; });
    if (text == image.segments.end()) throw macho::FormatError("guest exports require __TEXT");
    if (image.exports.size() > max_exports || image.dependencies.size() > 4096 || image.symbols.size() > 100000)
        throw macho::FormatError("guest image export/dependency limit exceeded");
    std::size_t names = name_bytes_;
    count_name(names, main ? "<main>" : image.install_name);
    for (const auto& dependency : image.dependencies) count_name(names, dependency.install_name);
    if (image.has_export_trie) {
        for (const auto& symbol : image.exports) count_name(names, symbol.name);
    } else {
        for (const auto& symbol : image.symbols)
            if (macho::is_defined_external_symbol(symbol)) count_name(names, symbol.name);
    }
    std::vector<macho::Export> exports;
    if (image.has_export_trie) exports = image.exports;
    else {
        for (const auto& symbol : image.symbols) {
            if (!macho::is_defined_external_symbol(symbol)) continue;
            const bool absolute = (symbol.type & 0x0e) == 2;
            if (!absolute && (symbol.value < text->vm_address || symbol.section_index > image.sections.size()))
                throw macho::FormatError("guest nlist export has invalid section/address");
            exports.push_back({symbol.name, absolute ? symbol.value : symbol.value - text->vm_address,
                               absolute ? 2U : 0U});
        }
    }
    Module module{main ? "<main>" : image.install_name, image.dependencies, {}, main, global};
    for (const auto& dependency : module.dependencies) validate_name(dependency.install_name);
    for (const auto& symbol : exports) {
        validate_name(symbol.name);
        std::uint64_t address = 0;
        // TLS, absolute, reexports and resolvers retain their names but cannot
        // be resolved by this narrow lookup implementation.
        if ((symbol.flags & ~std::uint64_t{4}) == 0) {
            const auto original = add(text->vm_address, symbol.address);
            const bool contained = std::any_of(image.segments.begin(), image.segments.end(),
                [&](const macho::Segment& segment) {
                    return segment.name != "__PAGEZERO" && original >= segment.vm_address &&
                           original - segment.vm_address < segment.vm_size;
                });
            address = add(guest_text_base, symbol.address);
            if (!address || !contained || !memory_.allowed(address, 1, cpu::Access::read))
                throw macho::FormatError("export target is outside mapped guest image");
        }
        if (!module.exports.emplace(symbol.name, Entry{address, symbol.flags}).second)
            throw macho::FormatError("duplicate guest exported symbol");
    }
    append(std::move(module));
}

void GuestModuleRegistry::add_runtime_module(std::string_view install_name,
                                             std::span<const RuntimeExport> exports) {
    if (exports.size() > max_exports) throw macho::FormatError("runtime export limit exceeded");
    std::size_t names = name_bytes_;
    count_name(names, install_name);
    for (const auto& symbol : exports) count_name(names, symbol.name);
    Module module{std::string(install_name), {}, {}, false, true};
    for (const auto& symbol : exports) {
        validate_name(symbol.name);
        if (!symbol.address || !memory_.fetch(symbol.address)) throw macho::FormatError("runtime export is not guest RX code");
        if (!module.exports.emplace(symbol.name, Entry{symbol.address, 0}).second)
            throw macho::FormatError("duplicate guest runtime export");
    }
    append(std::move(module));
}

void GuestModuleRegistry::seal() {
    if (modules_.empty() || !modules_.front().main)
        throw macho::FormatError("guest module registry has no main executable");
    for (const auto& module : modules_) {
        for (const auto& dependency : module.dependencies) {
            const bool loaded = std::any_of(modules_.begin(), modules_.end(),
                [&](const Module& candidate) { return candidate.name == dependency.install_name; });
            if (!loaded && !dependency.weak)
                throw macho::FormatError("guest lookup dependency not loaded: " + dependency.install_name);
        }
    }
    sealed_ = true;
}

std::optional<std::uint64_t> GuestModuleRegistry::lookup(
    std::uint64_t handle, std::string_view c_symbol) const {
    if (c_symbol.size() > max_name || c_symbol.find('\0') != c_symbol.npos)
        throw macho::FormatError("guest dlsym name exceeds supported scope");
    if (handle != rtld_default && handle != rtld_main_only)
        throw macho::FormatError("guest dlsym handle/caller scope unsupported");
    if (modules_.empty() || !modules_.front().main || (handle == rtld_default && !sealed_))
        throw macho::FormatError("guest dlsym loaded-image scope is incomplete");
    const auto raw = "_" + std::string(c_symbol);
    for (const auto& module : modules_) {
        if (handle == rtld_main_only && !module.main) break;
        if (handle == rtld_default && !module.global) continue;
        const auto found = module.exports.find(raw);
        if (found == module.exports.end()) continue;
        if ((found->second.flags & ~std::uint64_t{4}) != 0)
            throw macho::FormatError("guest dlsym TLS/absolute/reexport/resolver unsupported");
        if (!memory_.allowed(found->second.address, 1, cpu::Access::read))
            throw macho::FormatError("guest dlsym export mapping no longer exists");
        return found->second.address;
    }
    return std::nullopt;
}

GuestDlState::GuestDlState(cpu::GuestMemory& memory, const GuestModuleRegistry& modules,
                         std::uint64_t error_base, std::size_t max_threads)
    : memory_(memory), modules_(modules), error_base_(error_base), max_threads_(max_threads) {
    if (max_threads == 0 || max_threads > 64 || error_base % cpu::GuestMemory::ios_page_size ||
        max_threads > (UINT64_MAX - error_base) / cpu::GuestMemory::ios_page_size)
        throw macho::FormatError("invalid guest dlerror buffer configuration");
    errors_.reserve(max_threads);
}

GuestDlState::ThreadError& GuestDlState::state(std::uint64_t thread) {
    for (auto& error : errors_) if (error.thread == thread) return error;
    if (errors_.size() == max_threads_) throw macho::FormatError("guest dlerror thread limit exceeded");
    const auto address = error_base_ + errors_.size() * cpu::GuestMemory::ios_page_size;
    cpu::GuestMemory::MappingJournal journal(memory_);
    const auto rw = cpu::bits(cpu::Access::read) | cpu::bits(cpu::Access::write);
    if (!journal.map(address, cpu::GuestMemory::ios_page_size, rw, true))
        throw macho::FormatError("guest dlerror buffer is not available");
    errors_.push_back({thread, address, false});
    journal.commit();
    return errors_.back();
}

std::string GuestDlState::name(std::uint64_t address) const {
    if (!address) throw macho::FormatError("guest dlsym name is NULL");
    std::string result;
    for (std::size_t i = 0; i <= max_name; ++i) {
        const auto value = memory_.read(add(address, i), 1);
        if (!value) throw macho::FormatError("guest dlsym name is unreadable");
        if (*value == 0) return result;
        result.push_back(static_cast<char>(*value));
    }
    throw macho::FormatError("guest dlsym name is not bounded/NUL-terminated");
}

std::uint64_t GuestDlState::dlsym(std::uint64_t thread, std::uint64_t handle,
                                 std::uint64_t guest_name) {
    const auto symbol = name(guest_name);
    const auto target = modules_.lookup(handle, symbol);
    auto& error = state(thread);
    if (target) {
        error.pending = false;
        return *target;
    }
    const auto message = "dlsym: symbol not found: " + symbol;
    if (!memory_.copy_to(error.address, std::as_bytes(std::span(message.c_str(), message.size() + 1))))
        throw macho::FormatError("guest dlerror buffer is not writable");
    error.pending = true;
    return 0;
}

std::uint64_t GuestDlState::dlerror(std::uint64_t thread) {
    auto& error = state(thread);
    if (!error.pending) return 0;
    if (!memory_.allowed(error.address, 1, cpu::Access::read))
        throw macho::FormatError("guest dlerror buffer is not readable");
    error.pending = false;
    return error.address;
}

}
