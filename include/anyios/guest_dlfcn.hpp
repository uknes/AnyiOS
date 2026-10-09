#pragma once

#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace anyios::dyld {

inline constexpr std::uint64_t rtld_default = UINT64_MAX - 1;
inline constexpr std::uint64_t rtld_main_only = UINT64_MAX - 4;

struct RuntimeExport {
    std::string name; // Mach-O spelling, including the leading underscore.
    std::uint64_t address;
};

// Snapshots of already mapped images, in their actual load order.
class GuestModuleRegistry {
public:
    explicit GuestModuleRegistry(const cpu::GuestMemory& memory) : memory_(memory) {}
    void add_image(const macho::Image& image, std::uint64_t guest_text_base,
                   bool main = false, bool global = true);
    void add_runtime_module(std::string_view install_name,
                            std::span<const RuntimeExport> exports);
    void seal();
    std::optional<std::uint64_t> lookup(std::uint64_t handle,
                                      std::string_view c_symbol) const;
    std::size_t size() const { return modules_.size(); }

private:
    struct Entry { std::uint64_t address; std::uint64_t flags; };
    struct Module {
        std::string name;
        std::vector<macho::Dependency> dependencies;
        std::unordered_map<std::string, Entry> exports;
        bool main = false;
        bool global = true;
    };
    void append(Module module);
    const cpu::GuestMemory& memory_;
    std::vector<Module> modules_;
    std::size_t export_count_ = 0;
    std::size_t name_bytes_ = 0;
    bool sealed_ = false;
};

// Error buffers belong to explicit guest thread IDs, never host TLS.
class GuestDlState {
public:
    GuestDlState(cpu::GuestMemory& memory, const GuestModuleRegistry& modules,
                 std::uint64_t error_base, std::size_t max_threads = 16);
    std::uint64_t dlsym(std::uint64_t thread, std::uint64_t handle,
                        std::uint64_t guest_name);
    std::uint64_t dlerror(std::uint64_t thread);

private:
    struct ThreadError { std::uint64_t thread; std::uint64_t address; bool pending; };
    ThreadError& state(std::uint64_t thread);
    std::string name(std::uint64_t address) const;
    cpu::GuestMemory& memory_;
    const GuestModuleRegistry& modules_;
    std::uint64_t error_base_;
    std::size_t max_threads_;
    std::vector<ThreadError> errors_;
};

}
