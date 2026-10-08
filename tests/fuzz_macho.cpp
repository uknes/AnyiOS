#include <anyios/macho.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        const auto bytes = std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(data), size);
        static_cast<void>(anyios::macho::inspect(bytes));
    } catch (const anyios::macho::FormatError&) {
    }
    return 0;
}
