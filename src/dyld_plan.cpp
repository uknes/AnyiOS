#include <anyios/dyld_plan.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace anyios::dyld {
namespace {

bool begins(std::string_view value, std::string_view prefix) {
    return value.starts_with(prefix);
}

std::string normalize(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.find('\\') != path.npos ||
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

LoadPlan plan_dependencies(std::span<const Module> modules,
                           std::string_view executable_path) {
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
    std::vector<std::uint8_t> state(modules.size());
    LoadPlan plan;
    std::size_t edges = 0;
    std::function<void(std::size_t, const std::vector<std::string>&, unsigned)> visit;

    visit = [&](std::size_t current,
                const std::vector<std::string>& inherited,
                unsigned depth) {
        if (depth > 64) throw macho::FormatError("dyld dependency depth limit exceeded");
        if (state[current] == 1) throw macho::FormatError("circular dyld dependency unsupported");
        if (state[current] == 2) return;
        state[current] = 1;
        const auto& module = modules[current];
        if (current != root->second && module.image.file_type != 6) {
            throw macho::FormatError("dyld dependency is not MH_DYLIB");
        }

        std::vector<std::string> runpaths;
        for (const auto& runpath : module.image.rpaths) {
            if (runpath.starts_with("@rpath/")) {
                throw macho::FormatError("nested @rpath is unsupported");
            }
            runpaths.push_back(expand(runpath, module.path, executable));
        }
        runpaths.insert(runpaths.end(), inherited.begin(), inherited.end());
        for (const auto& dependency : module.image.dependencies) {
            if (++edges > 16384) throw macho::FormatError("dyld dependency edge limit exceeded");
            const auto& name = dependency.install_name;
            if (name.empty()) throw macho::FormatError("empty dyld dependency");
            std::string resolved;
            if (begins(name, "@rpath/")) {
                for (const auto& runpath : runpaths) {
                    const auto candidate = join(runpath, std::string_view(name).substr(7));
                    if (index.contains(candidate)) {
                        resolved = candidate;
                        break;
                    }
                }
            } else if (begins(name, "@loader_path/") || begins(name, "@executable_path/")) {
                const auto candidate = expand(name, module.path, executable);
                if (index.contains(candidate)) resolved = candidate;
            } else {
                throw macho::FormatError("system or bare dylib paths are not implemented");
            }
            if (resolved.empty()) {
                if (dependency.weak) {
                    plan.missing_weak.push_back({module.path, name});
                    continue;
                }
                throw macho::FormatError("unresolved dyld dependency: " + name);
            }
            visit(index.at(resolved), runpaths, depth + 1);
        }
        state[current] = 2;
        plan.load_order.push_back(module.path);
    };
    visit(root->second, {}, 0);
    return plan;
}
}
