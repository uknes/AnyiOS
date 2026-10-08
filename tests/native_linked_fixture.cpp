#include <anyios/linked_pair.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/macho.hpp>

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#if !defined(_M_ARM64)
#error "This CI-only native linked-fixture executable requires Windows ARM64"
#endif

namespace {
constexpr std::size_t arena_size = 4 * 1024 * 1024;

class HostArena {
public:
    HostArena() {
        address_ = VirtualAlloc(nullptr, arena_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!address_) throw std::runtime_error("host ARM64 VM allocation failed");
    }
    HostArena(const HostArena&) = delete;
    HostArena& operator=(const HostArena&) = delete;
    ~HostArena() {
        if (address_) VirtualFree(address_, 0, MEM_RELEASE);
    }
    std::uint64_t base() const {
        return reinterpret_cast<std::uintptr_t>(address_);
    }
private:
    void* address_ = nullptr;
};

std::vector<std::byte> owned_image(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("project-owned linked fixture not found");
    const auto length = input.tellg();
    if (length <= 0 || length > 32 * 1024 * 1024) {
        throw std::runtime_error("invalid owned linked fixture length");
    }
    input.seekg(0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("cannot read linked fixture");
    }
    return bytes;
}

void publish_segment(const anyios::macho::Image& image,
                     std::uint64_t mapped_base,
                     const anyios::cpu::GuestMemory& staged) {
    const auto text = std::find_if(image.segments.begin(), image.segments.end(),
        [](const anyios::macho::Segment& segment) { return segment.name == "__TEXT"; });
    if (text == image.segments.end()) throw std::runtime_error("missing linked text segment");
    for (const auto& segment : image.segments) {
        if (segment.name == "__PAGEZERO" && segment.file_size == 0) continue;
        if (segment.vm_address < text->vm_address ||
            segment.vm_size > arena_size ||
            segment.file_size > segment.vm_size) {
            throw std::runtime_error("invalid native linked segment range");
        }
        const auto offset = segment.vm_address - text->vm_address;
        if (mapped_base > std::numeric_limits<std::uint64_t>::max() - offset) {
            throw std::runtime_error("native linked segment address overflow");
        }
        const auto address = mapped_base + offset;
        if (segment.file_size) {
            std::vector<std::byte> data(static_cast<std::size_t>(segment.file_size));
            if (!staged.copy_from(address, data)) {
                throw std::runtime_error("cannot copy validated guest segment");
            }
            std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
                        data.data(), data.size());
        }
        DWORD native_protect = PAGE_NOACCESS;
        const auto protection = segment.init_protection;
        if (protection == 5) native_protect = PAGE_EXECUTE_READ;
        else if (protection == 3) native_protect = PAGE_READWRITE;
        else if (protection == 1) native_protect = PAGE_READONLY;
        else throw std::runtime_error("unsupported native image permission");
        DWORD previous = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
                            static_cast<std::size_t>(segment.vm_size),
                            native_protect, &previous)) {
            throw std::runtime_error("native linked segment permission transition failed");
        }
        if (native_protect == PAGE_EXECUTE_READ &&
            !FlushInstructionCache(GetCurrentProcess(),
                                   reinterpret_cast<const void*>(
                                       static_cast<std::uintptr_t>(address)),
                                   static_cast<std::size_t>(segment.vm_size))) {
            throw std::runtime_error("host ARM64 instruction-cache flush failed");
        }
    }
}

void verify(const std::vector<std::byte>& app, const std::vector<std::byte>& library) {
    HostArena arena;
    anyios::cpu::GuestMemory staged(arena.base(), arena_size);
    const auto app_base = arena.base() + 0x10000;
    const auto lib_base = arena.base() + 0x80000;
    const auto result = anyios::loader::stage_owned_linked_pair(
        app, library, staged, app_base, lib_base);
    publish_segment(anyios::macho::inspect(library), lib_base, staged);
    publish_segment(anyios::macho::inspect(app), app_base, staged);
    if (result.entry < app_base || result.entry - app_base >= arena_size) {
        throw std::runtime_error("invalid native ARM64 linked entry");
    }
    using OwnedMain = int (*)();
    const auto main = reinterpret_cast<OwnedMain>(
        static_cast<std::uintptr_t>(result.entry));
    const int value = main();
    if (value != 42) throw std::runtime_error("real native ARM64 linked call returned wrong value");
    std::cout << "Windows ARM64 natively executed genuine owned iPhoneOS app -> dylib call: 42\n";
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: anyios-native-linked-owned RuntimeApp libRuntimeWidget.dylib\n";
        return 2;
    }
    try {
        verify(owned_image(argv[1]), owned_image(argv[2]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native ARM64 linked fixture failed: " << error.what() << '\n';
        return 1;
    }
}
