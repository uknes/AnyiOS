#include <anyios/cpu_backend.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/guest_os_log.hpp>
#include <anyios/guest_environ.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>
#include <anyios/legacy_bind.hpp>
#include <anyios/process_bootstrap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t kMaxFile = 128U * 1024U * 1024U;
constexpr std::size_t kMaxImports = 8192;
constexpr std::size_t kMaxSteps = 10000;
constexpr std::uint64_t kStub = 0x10000000;
constexpr std::uint64_t kStack = 0x14000000;
constexpr std::uint64_t kReturn = 0x17000000;
constexpr std::size_t kStubBytes = 0x20000;

std::vector<std::byte> read_app(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("cannot read source-verified app Mach-O");
    const auto len = f.tellg();
    if (len <= 0 || len > static_cast<std::streamoff>(kMaxFile)) {
        throw std::runtime_error("original app exceeds 128 MiB bound");
    }
    f.seekg(0);
    std::vector<std::byte> data(static_cast<std::size_t>(len));
    if (!f.read(reinterpret_cast<char*>(data.data()),
                static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("original Mach-O read incomplete");
    }
    return data;
}

void store_instruction(std::array<std::byte, kStubBytes>& dest,
                       std::size_t offset, std::uint32_t word) {
    if (offset > dest.size() - 4) throw std::runtime_error("import stub overflow");
    for (std::size_t i = 0; i < 4; ++i) {
        dest[offset + i] = std::byte{static_cast<unsigned char>((word >> (8 * i)) & 255U)};
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: anyios-general-entry-probe <unchanged-arm64-ios-Mach-O>\n";
        return 2;
    }
    try {
        const auto data = read_app(argv[1]);
        const auto image = anyios::macho::inspect(data);
        if (image.file_type != 2 || image.is_encrypted) {
            throw std::runtime_error("only unprotected ARM64 MH_EXECUTE accepted");
        }
        auto import_names=image.chained_imports;
        if(image.legacy_dyld) {
            if(image.has_chained_fixups)
                throw std::runtime_error("mixed legacy and chained fixups unsupported");
            for(const auto& site:anyios::dyld::inspect_legacy_eager_bind_sites(data,image))
                import_names.push_back(site.symbol);
            for(const auto& site:anyios::dyld::inspect_legacy_lazy_bind_sites(data,image))
                import_names.push_back(site.symbol);
        }
        if (import_names.empty() || import_names.size() > kMaxImports) {
            throw std::runtime_error("unsupported external app import site count");
        }
        std::vector<std::uint64_t> imports;
        imports.reserve(import_names.size());
        for (std::size_t i = 0; i < import_names.size(); ++i) {
            imports.push_back(kStub + static_cast<std::uint64_t>(i * 16));
        }

        // Research-only in-process guest interpreter. It is NOT an isolation
        // boundary for untrusted commercial binaries.
        anyios::cpu::GuestMemory memory(0x10000, 384U * 1024U * 1024U);
        const auto loaded = anyios::loader::stage_linked_image(
            data, memory, 0x10000, imports,
            anyios::loader::LinkedImageOptions{true, nullptr, image.legacy_dyld.has_value()});
        const auto rx = anyios::cpu::bits(anyios::cpu::Access::read) |
                        anyios::cpu::bits(anyios::cpu::Access::execute);
        if (!memory.map_ios(kStub, kStubBytes, rx)) {
            throw std::runtime_error("mapped guest overlaps bounded import trap area");
        }
        std::array<std::byte, kStubBytes> traps{};
        for (std::size_t i = 0; i < imports.size(); ++i) {
            const auto offset = i * 16;
            store_instruction(traps, offset, 0xd2800010U |
                               (static_cast<std::uint32_t>(i) << 5));
            store_instruction(traps, offset + 4, 0xd4001001U);
            store_instruction(traps, offset + 8, 0xd65f03c0U);
            store_instruction(traps, offset + 12, 0xd4200000U);
        }
        if (!memory.load(kStub, traps)) {
            throw std::runtime_error("could not install bounded original guest traps");
        }

        const std::array<std::string_view, 1> args{"UnchangediOSApp"};
        const std::array<std::string_view, 0> env{};
        const std::array<std::string_view, 1> apple{
            "executable_path=/Applications/Unchanged.app/UnchangediOSApp"
        };
        const auto start = anyios::loader::prepare_owned_process_stack(
            memory, kStack, 0x10000, args, env, apple);
        anyios::darwin::GuestOsLogRegistry os_logs(memory, 0x16000000);
        const anyios::darwin::GuestEnvironment guest_environ(memory, start.envp);
        auto cpu = anyios::cpu::make_dynarmic_backend(memory);
        anyios::cpu::CpuState state{};
        state.pc = loaded.guest_entry;
        state.sp = start.sp;
        state.x[0] = start.argc;
        state.x[1] = start.argv;
        state.x[2] = start.envp;
        state.x[3] = start.apple;
        state.x[30] = kReturn;
        cpu->set_state(state);
        std::cout << "ENTRY_PROBE=original-unchanged-arm64-instructions\n"
                  << "ENGINE=Dynarmic-x86-64\n"
                  << "IMPORT_TARGETS=unresolved-guest-diagnostic-traps\n"
                  << "LEGACY_LAZY_POLICY=eager-diagnostic-prebinding\n"
                  << "DEPENDENT_DYLIBS=not-executed\n"
                  << "INITIALIZERS=not-executed\n"
                  << "WINDOW=not-created\n";
        for (std::size_t i = 0; i < kMaxSteps; ++i) {
            const auto event = cpu->step();
            const auto at = cpu->state();
            if (event.kind == anyios::cpu::CpuEventKind::stepped && at.pc != kReturn) {
                continue;
            }
            std::cout << "GUEST_STEPS=" << (i + 1) << "\n";
            if (event.kind == anyios::cpu::CpuEventKind::svc &&
                event.svc_immediate == 0x80 && at.x[16] < imports.size() &&
                at.pc == imports[static_cast<std::size_t>(at.x[16])] + 8) {
                const auto& symbol = import_names[
                    static_cast<std::size_t>(at.x[16])];
                if (symbol == "_getenv") {
                    const auto result = guest_environ.lookup(at.x[0]);
                    if (!result) {
                        std::cout << "FIRST_RUNTIME_BLOCKER=_getenv\n"
                                  << "REASON=invalid-original-guest-envp-or-name\n";
                        return 0;
                    }
                    auto resumed = at;
                    resumed.x[0] = *result;
                    cpu->set_state(resumed);
                    std::cout << "SUPPORTED_NARROW_IMPORT=_getenv\n"
                              << "ENV_SCOPE=original-guest-process-envp-only\n";
                    continue;
                }
                if (symbol == "_os_log_create") {
                    const auto guest_log = os_logs.create(at.x[0], at.x[1]);
                    if (!guest_log) {
                        std::cout << "FIRST_RUNTIME_BLOCKER=_os_log_create\n"
                                  << "REASON=invalid-bounded-guest-C-string-arguments\n";
                        return 0;
                    }
                    auto resumed = at;
                    resumed.x[0] = *guest_log;
                    cpu->set_state(resumed);
                    std::cout << "SUPPORTED_NARROW_IMPORT=_os_log_create\n"
                              << "OS_LOG_SCOPE=opaque-guest-token-only\n";
                    continue;
                }
                std::cout << "FIRST_RUNTIME_BLOCKER=" << symbol
                          << "\nRESULT=unsupported-framework-import\n";
                return 0;
            }
            std::cout << "FIRST_RUNTIME_BLOCKER=unrecognized-guest-event\n"
                      << "GUEST_PC=" << at.pc << "\n"
                      << "EVENT_DIAGNOSTIC=" << event.diagnostic << "\n";
            return 3;
        }
        std::cout << "FIRST_RUNTIME_BLOCKER=instruction-budget\n"
                  << "GUEST_STEPS=" << kMaxSteps << "\n";
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "ENTRY_PROBE=not-started\n"
                  << "FIRST_LOADER_BLOCKER=" << e.what() << "\n"
                  << "WINDOW=not-created\n";
        return 3;
    }
}
