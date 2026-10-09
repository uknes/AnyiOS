#include <anyios/objc_signature.hpp>

namespace anyios::darwin {

bool supported_bool_launch_abi(std::string_view encoding) noexcept {
    // Clang's signed-char BOOL or C99 _Bool return; compiler-derived
    // self/_cmd and two object arguments at 64-bit Apple ABI slots.
    // Refuse qualifiers, blocks, structs, variadic or unknown offsets.
    return encoding == "c32@0:8@16@24" ||
           encoding == "B32@0:8@16@24";
}

} // namespace anyios::darwin
