#include <anyios/cpu_backend.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>
#include <anyios/process_bootstrap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::vector<std::byte> read_vetted_external(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("missing pinned upstream iOS app");
    const auto n = input.tellg();
    if (n <= 0 || n > 2 * 1024 * 1024) {
        throw std::runtime_error("external app exceeds 2 MiB entry-probe limit");
    }
    input.seekg(0);
    std::vector<std::byte> file(static_cast<std::size_t>(n));
    if (!input.read(reinterpret_cast<char*>(file.data()),
                    static_cast<std::streamsize>(file.size()))) {
        throw std::runtime_error("cannot read pinned upstream iOS app");
    }
    return file;
}

void emit32(std::array<std::byte, 0x4000>& out,
            std::size_t at, std::uint32_t instruction) {
    if (at > out.size() - 4) throw std::runtime_error("guest thunk page overflow");
    for (unsigned i = 0; i != 4; ++i) {
        out[at + i] = std::byte((instruction >> (8 * i)) & 255);
    }
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: anyios-app-entry-probe <PINNED-MIT-app-Mach-O>\n";
        return 2;
    }
    try {
        constexpr std::uint64_t kStub = 0x80000;
        constexpr std::uint64_t kReturn = 0x400000;
        auto data = read_vetted_external(argv[1]);
        const auto image = anyios::macho::inspect(data);
        if (image.file_type != 2 || image.is_encrypted ||
            image.chained_imports.empty() || image.chained_imports.size() > 256) {
            throw std::runtime_error("unsupported or unbounded external app import shape");
        }
        std::vector<std::uint64_t> imports;
        imports.reserve(image.chained_imports.size());
        for (std::size_t i = 0; i < image.chained_imports.size(); ++i) {
            imports.push_back(kStub + i * 16);
        }
        anyios::cpu::GuestMemory memory(0x10000, 16 * 1024 * 1024);
        const auto loaded = anyios::loader::stage_linked_image(
            data, memory, 0x10000, imports,
            anyios::loader::LinkedImageOptions{true, nullptr});
        const auto rx = anyios::cpu::bits(anyios::cpu::Access::read) |
                        anyios::cpu::bits(anyios::cpu::Access::execute);
        if (!memory.map_ios(kStub, 0x4000, rx)) {
            throw std::runtime_error("cannot map fail-closed guest import traps");
        }
        std::array<std::byte, 0x4000> stubs{};
        for (std::size_t i = 0; i < imports.size(); ++i) {
            const auto at = i * 16;
            // movz x16, #index; svc #0x80; ret; brk #0 (unreachable)
            emit32(stubs, at + 0, 0xd2800010U |
                   (static_cast<std::uint32_t>(i) << 5));
            emit32(stubs, at + 4, 0xd4001001U);
            emit32(stubs, at + 8, 0xd65f03c0U);
            emit32(stubs, at + 12, 0xd4200000U);
        }
        if (!memory.load(kStub, stubs)) {
            throw std::runtime_error("could not initialize bounded guest thunks");
        }
        const std::array<std::string_view, 1> arguments{"BitriseSimpleObjC"};
        const std::array<std::string_view, 0> environment{};
        const std::array<std::string_view, 1> apple{
            "executable_path=/Applications/BitriseSimpleObjC.app/BitriseSimpleObjC"
        };
        const auto process = anyios::loader::prepare_owned_process_stack(
            memory, 0x320000, 0x10000, arguments, environment, apple);
        auto cpu = anyios::cpu::make_dynarmic_backend(memory);
        anyios::cpu::CpuState state{};
        state.pc = loaded.guest_entry;
        state.sp = process.sp;
        state.x[0] = process.argc;
        state.x[1] = process.argv;
        state.x[2] = process.envp;
        state.x[3] = process.apple;
        state.x[30] = kReturn;
        cpu->set_state(state);

        // This intentionally omits dyld ObjC registration/initializers.
        // It is a bounded ENTRY-ONLY diagnostic, NOT a correct app launch.
        std::cout << "ENTRY_PROBE=original-external-ios-arm64-instructions\n"
                  << "INITIALIZERS=not-executed\n"
                  << "FRAMEWORK_RUNTIME=not-provided\n";
        // The only permitted ObjC runtime subset here is an *empty* pool:
        // a push yields a unique opaque guest token; pop must match LIFO.
        // No autoreleased objects, retain/release, nested drain callbacks,
        // exceptions, or cross-thread semantics are claimed.
        std::vector<std::uint64_t> empty_pools;
        std::uint64_t budget = 10000;
        while (budget--) {
            const auto event = cpu->step();
            state = cpu->state();
            if (event.kind == anyios::cpu::CpuEventKind::stepped) {
                if (state.pc == kReturn) {
                    std::cerr << "ENTRY_PROBE=returned-without-real-frameworks\n";
                    return 3;
                }
                continue;
            }
            if (event.kind != anyios::cpu::CpuEventKind::svc ||
                event.svc_immediate != 0x80 ||
                state.x[16] >= imports.size() ||
                state.pc != imports[static_cast<std::size_t>(state.x[16])] + 8) {
                std::cerr << "ENTRY_PROBE=unexpected-event\nGUEST_PC="
                          << state.pc << "\nEVENT_DIAGNOSTIC=" << event.diagnostic << "\n";
                return 3;
            }
            const auto& symbol = image.chained_imports[static_cast<std::size_t>(state.x[16])];
            if (symbol == "_objc_autoreleasePoolPush") {
                if (empty_pools.size() >= 16) {
                    std::cerr << "ENTRY_PROBE=pool-depth-exceeded\n";
                    return 3;
                }
                const std::uint64_t token = 0x300000 + empty_pools.size() * 16;
                empty_pools.push_back(token);
                state.x[0] = token;
                cpu->set_state(state);
                std::cout << "SUPPORTED_NARROW_IMPORT=_objc_autoreleasePoolPush"
                          << "\nPOOL_SCOPE=empty-only\n";
                continue;
            }
            if (symbol == "_objc_autoreleasePoolPop") {
                if (empty_pools.empty() || empty_pools.back() != state.x[0]) {
                    std::cerr << "ENTRY_PROBE=invalid-empty-pool-token\n";
                    return 3;
                }
                empty_pools.pop_back();
                cpu->set_state(state);
                std::cout << "SUPPORTED_NARROW_IMPORT=_objc_autoreleasePoolPop"
                          << "\nPOOL_SCOPE=empty-only\n";
                continue;
            }
            std::cout << "FIRST_RUNTIME_BLOCKER=" << symbol
                      << "\nEXECUTION=stopped-at-unimplemented-import\n";
            return 0;
        }
        std::cerr << "ENTRY_PROBE=instruction-budget-exhausted\n";
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "ENTRY_PROBE=blocked\nFIRST_LOADER_OR_GUEST_ERROR="
                  << e.what() << '\n';
        return 3;
    }
}
