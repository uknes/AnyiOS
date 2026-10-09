#pragma once

#include <anyios/macho.hpp>

#include <span>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace anyios::dyld {

struct Module {
    std::string path;
    macho::Image image;
};

struct MissingWeak {
    std::string loader;
    std::string install_name;
};

struct LoadPlan {
    std::vector<std::string> load_order;
    std::vector<MissingWeak> missing_weak;
};

struct UnresolvedDependency {
    std::string loader;
    std::string install_name;
    bool weak;
};

struct ExternalRunpath {
    std::string loader;
    std::string path;
};

struct DependencyDiscovery {
    std::vector<Module> modules;
    LoadPlan plan;
    std::vector<UnresolvedDependency> unresolved;
    std::vector<ExternalRunpath> external_runpaths;
};

// The reader accepts canonical paths relative to a caller-owned bundle root.
// nullopt means absent; malformed or inaccessible content must throw.
using ModuleReader = std::function<std::optional<macho::Image>(std::string_view)>;
DependencyDiscovery discover_dependencies(std::string_view executable_path,
                                          const ModuleReader& reader);

LoadPlan plan_dependencies(std::span<const Module> modules,
                           std::string_view executable_path);

}
