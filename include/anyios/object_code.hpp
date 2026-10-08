#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace anyios::loader {

struct ObjectFunction {
    std::vector<std::byte> code;
};

ObjectFunction extract_object_function(std::span<const std::byte> input,
                                       std::string_view name);
}
