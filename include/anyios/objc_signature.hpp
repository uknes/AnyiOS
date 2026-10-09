#pragma once

#include <string_view>

namespace anyios::darwin {

// Exact, deliberately narrow compiler-emitted ObjC method signature contract
// for the already-inspected pinned AppDelegate diagnostic. The encoded ABI is
// BOOL method(id self, SEL _cmd, id application, id launchOptions).
// This is not generic Objective-C type decoding or objc_msgSend support.
bool supported_bool_launch_abi(std::string_view encoding) noexcept;

} // namespace anyios::darwin
