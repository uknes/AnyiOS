#include <anyios/objc_signature.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main() {
    using anyios::darwin::supported_bool_launch_abi;
    if (!supported_bool_launch_abi("c32@0:8@16@24") ||
        !supported_bool_launch_abi("B32@0:8@16@24")) {
        throw std::runtime_error("supported original BOOL callback ABI refused");
    }
    constexpr std::array<std::string_view, 14> invalid{
        "", "v32@0:8@16@24", "i32@0:8@16@24",
        "c24@0:8@16", "c40@0:8@16@24@32",
        "c32@0:8@16^v24", "c32@0:8@?16@24",
        "c32@0:8@16@25", "c32@0:8@16@24junk",
        "c32@0:8@16@24\n", "c32@0:8@16@",
        "c32@0:8{_CGRect=}16@24", "c32@0:8f16@24",
        "c32@0:8@16@-1"
    };
    for (const auto types : invalid) {
        if (supported_bool_launch_abi(types)) {
            throw std::runtime_error("unsupported ObjC ABI was accepted");
        }
    }
    std::cout << "Restricted original guest BOOL delegate method ABI passed\n";
}
