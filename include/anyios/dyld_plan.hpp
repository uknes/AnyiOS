#pragma once

#include <anyios/macho.hpp>

#include <span>
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

LoadPlan plan_dependencies(std::span<const Module> modules,
                           std::string_view executable_path);

}
