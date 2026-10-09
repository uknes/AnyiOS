#include <anyios/dyld_plan.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace anyios::dyld {
namespace {

bool begins(std::string_view value, std::string_view prefix) {
    return value.starts_with(prefix);
}

std::string normalize(std::string_view path) {
    if (path.empty() || path.size() > 4096 || path.front() == '/' || path.find('\\') != path.npos ||
        path.find(':') != path.npos || path.find('\0') != path.npos) {
        throw macho::FormatError("invalid bundle-relative path");
    }
    std::vector<std::string> parts;
    for (std::size_t i = 0; i < path.size();) {
        const auto end = path.find('/', i);
        const auto token = path.substr(i, end == path.npos ? end : end - i);
        if (token == "..") {
            if (parts.empty()) throw macho::FormatError("dyld path escapes module registry");
            parts.pop_back();
        } else if (!token.empty() && token != ".") {
            parts.emplace_back(token);
        }
        if (end == path.npos) break;
        i = end + 1;
    }
    if (parts.empty()) throw macho::FormatError("empty normalized dyld path");
    std::string result = parts.front();
    for (std::size_t i = 1; i < parts.size(); ++i) result += "/" + parts[i];
    return result;
}

std::string directory(std::string_view path) {
    const auto pos = path.find_last_of('/');
    return pos == path.npos ? "" : std::string(path.substr(0, pos));
}

std::string join(std::string_view base, std::string_view suffix) {
    return normalize(base.empty() ? std::string(suffix) :
                     std::string(base) + "/" + std::string(suffix));
}

std::string expand(std::string_view value,
                   std::string_view owner,
                   std::string_view executable) {
    if (value == "@loader_path") return normalize(directory(owner));
    if (value == "@executable_path") return normalize(directory(executable));
    if (begins(value, "@loader_path/")) {
        return join(directory(owner), value.substr(13));
    }
    if (begins(value, "@executable_path/")) {
        return join(directory(executable), value.substr(17));
    }
    if (value.starts_with("@") || value.starts_with('/')) {
        throw macho::FormatError("unsupported dyld install-name prefix");
    }
    return normalize(value);
}

}

namespace {
DependencyDiscovery walk(std::vector<Module> modules, std::string_view executable_path,
                         const ModuleReader* reader) {
    if (modules.empty() || modules.size() > 4096) {
        throw macho::FormatError("invalid dyld module count");
    }
    const auto executable = normalize(executable_path);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < modules.size(); ++i) {
        const auto path = normalize(modules[i].path);
        if (path != modules[i].path || !index.emplace(path, i).second) {
            throw macho::FormatError("duplicate or noncanonical dyld module path");
        }
    }
    const auto root = index.find(executable);
    if (root == index.end() || modules[root->second].image.file_type != 2) {
        throw macho::FormatError("main dyld executable is missing or invalid");
    }
    const auto root_index = root->second;
    std::vector<std::uint8_t> state(modules.size());
    std::unordered_set<std::string> absent;
    LoadPlan plan;
    std::vector<UnresolvedDependency> unresolved;
    std::size_t edges = 0, reads = 0;
    auto locate = [&](const std::string& candidate) -> std::optional<std::size_t> {
        if (const auto found = index.find(candidate); found != index.end()) return found->second;
        if (!reader || absent.contains(candidate)) return std::nullopt;
        if (++reads > 65536) throw macho::FormatError("dyld candidate read limit exceeded");
        auto image = (*reader)(candidate);
        if (!image) { absent.insert(candidate); return std::nullopt; }
        if (modules.size() >= 4096) throw macho::FormatError("dyld module count limit exceeded");
        const auto slot = modules.size();
        modules.push_back({candidate, std::move(*image)});
        state.push_back(0);
        index.emplace(candidate, slot);
        return slot;
    };
    std::function<void(std::size_t, const std::vector<std::string>&, unsigned)> visit;
    visit = [&](std::size_t current, const std::vector<std::string>& inherited, unsigned depth) {
        if (depth > 64) throw macho::FormatError("dyld dependency depth limit exceeded");
        if (state[current] == 1) throw macho::FormatError("circular dyld dependency unsupported");
        if (state[current] == 2) return;
        state[current] = 1;
        if (current != root_index && modules[current].image.file_type != 6) {
            throw macho::FormatError("dyld dependency is not MH_DYLIB");
        }
        // Discovery may grow modules. Own the strings used across reader calls.
        const auto owner = modules[current].path;
        const auto dependencies = modules[current].image.dependencies;
        std::vector<std::string> runpaths;
        for (const auto& runpath : modules[current].image.rpaths) {
            if (runpath.starts_with("@rpath/")) {
                throw macho::FormatError("nested @rpath is unsupported");
            }
            if (runpaths.size() >= 512) throw macho::FormatError("dyld runpath limit exceeded");
            runpaths.push_back(expand(runpath, owner, executable));
        }
        if (inherited.size() > 512 - runpaths.size()) {
            throw macho::FormatError("dyld inherited runpath limit exceeded");
        }
        runpaths.insert(runpaths.end(), inherited.begin(), inherited.end());
        for (const auto& dependency : dependencies) {
            if (++edges > 16384) throw macho::FormatError("dyld dependency edge limit exceeded");
            const auto& name = dependency.install_name;
            if (name.empty() || name.size() > 4096 || name.find('\0') != name.npos ||
                name.find('\\') != name.npos || name.find(':') != name.npos) {
                throw macho::FormatError("invalid dyld dependency name");
            }
            std::optional<std::size_t> resolved;
            if (begins(name, "@rpath/")) {
                for (const auto& runpath : runpaths) {
                    resolved = locate(join(runpath, std::string_view(name).substr(7)));
                    if (resolved) break;
                }
            } else if (begins(name, "@loader_path/") || begins(name, "@executable_path/")) {
                resolved = locate(expand(name, owner, executable));
            } else if (reader && name.front() == '/') {
                // System images are requirements, never host paths or guessed modules.
            } else {
                throw macho::FormatError("system or bare dylib paths are not implemented");
            }
            if (!resolved) {
                if (reader) unresolved.push_back({owner, name, dependency.weak});
                if (dependency.weak) {
                    plan.missing_weak.push_back({owner, name});
                    continue;
                }
                if (reader) continue;
                throw macho::FormatError("unresolved dyld dependency: " + name);
            }
            visit(*resolved, runpaths, depth + 1);
        }
        state[current] = 2;
        plan.load_order.push_back(owner);
    };
    visit(root_index, {}, 0);
    return {std::move(modules), std::move(plan), std::move(unresolved)};
}
}

LoadPlan plan_dependencies(std::span<const Module> modules, std::string_view executable_path) {
    return walk(std::vector<Module>(modules.begin(), modules.end()), executable_path, nullptr).plan;
}

DependencyDiscovery discover_dependencies(std::string_view executable_path,
                                          const ModuleReader& reader) {
    if (!reader) throw macho::FormatError("missing dyld module reader");
    const auto path = normalize(executable_path);
    auto image = reader(path);
    if (!image) throw macho::FormatError("main dyld executable is missing or invalid");
    std::vector<Module> modules;
    modules.push_back({path, std::move(*image)});
    return walk(std::move(modules), path, &reader);
}
}
