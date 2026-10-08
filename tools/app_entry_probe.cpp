#include <anyios/abi_thunk.hpp>
#include <anyios/cpu_backend.hpp>
#include <anyios/guest_memory.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>
#include <anyios/objc_identity.hpp>
#include <anyios/objc_objects.hpp>
#include <anyios/objc_registry.hpp>
#include <anyios/objc_selectors.hpp>
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
        const anyios::darwin::ObjcIdentityProbe objc(image, memory, loaded.guest_base);
        const anyios::darwin::GuestObjcClassRegistry registry(objc, memory);
        anyios::darwin::GuestObjcSelectorRegistry selectors(objc);
        std::cout << "OBJC_LOCAL_CLASSES_REGISTERED=" << registry.size() << "\n"
                  << "OBJC_UNRESOLVED_CLASS_RECORDS=" << registry.unresolved_count() << "\n";
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
        std::optional<std::uint64_t> bridged_delegate_class;
        std::optional<std::string> bridged_delegate_name;
        constexpr std::uint64_t kProbeNameHandle = 0x360000;
        constexpr std::uint64_t kDiagnosticDelegate = 0x380000;
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
            if (symbol == "_objc_msgSend") {
                const auto selector = objc.selector_name(state.x[1]);
                const auto canonical = selectors.intern_compiled_selector(state.x[1]);
                const auto identity = canonical
                    ? objc.invoke_class_identity(state.x[0], *canonical)
                    : std::nullopt;
                if (identity) {
                    state.x[0] = *identity;
                    cpu->set_state(state);
                    std::cout << "SUPPORTED_NARROW_IMPORT=_objc_msgSend"
                              << "\nMETHOD=+class-local-identity\n"
                              << "SELECTOR_IDENTITY=validated-guest-methname\n";
                    continue;
                }
                std::cout << "FIRST_RUNTIME_BLOCKER=_objc_msgSend"
                          << "\nUNSUPPORTED_SELECTOR="
                          << selector.value_or("(unresolved-selector)")
                          << "\nEXECUTION=stopped-at-unimplemented-import\n";
                return 0;
            }
            if (symbol == "_objc_getClass") {
                const auto cls = registry.find_guest_name(state.x[0]);
                if (!cls) {
                    std::cout << "FIRST_RUNTIME_BLOCKER=_objc_getClass"
                              << "\nREASON=class-not-registered-locally"
                              << "\nEXECUTION=stopped-at-unimplemented-import\n";
                    return 0;
                }
                state.x[0] = *cls;
                cpu->set_state(state);
                std::cout << "SUPPORTED_NARROW_IMPORT=_objc_getClass"
                          << "\nCLASS_SCOPE=local-compiler-metadata-only\n";
                continue;
            }
            if (symbol == "_NSStringFromClass") {
                const auto name = objc.local_class_name(state.x[0]);
                if (!name || registry.find(*name) != state.x[0] || name->size() > 127) {
                    std::cout << "FIRST_RUNTIME_BLOCKER=_NSStringFromClass"
                              << "\nREASON=unresolvable-class-metadata"
                              << "\nEXECUTION=stopped-at-unimplemented-import\n";
                    return 0;
                }
                // Narrow ABI hand-off token for the immediately following
                // UIApplicationMain probe. This is NOT an NSString object;
                // any attempted guest dereference would fault closed.
                bridged_delegate_class = state.x[0];
                bridged_delegate_name = *name;
                state.x[0] = kProbeNameHandle;
                cpu->set_state(state);
                std::cout << "DIAGNOSTIC_BRIDGE=_NSStringFromClass"
                          << "\nCLASS_NAME=" << *name
                          << "\nOBJECT_SCOPE=probe-only-token-not-NSString\n";
                continue;
            }
            if (symbol == "_UIApplicationMain") {
                if (!bridged_delegate_class || !bridged_delegate_name ||
                    state.x[3] != kProbeNameHandle) {
                    std::cout << "FIRST_RUNTIME_BLOCKER=_UIApplicationMain"
                              << "\nREASON=invalid-delegate-name-proxy"
                              << "\nEXECUTION=stopped-at-unimplemented-import\n";
                    return 0;
                }
                const auto did_launch = registry.resolve_local_instance_method(
                    *bridged_delegate_class,
                    "application:didFinishLaunchingWithOptions:");
                if (!did_launch) {
                    std::cout << "FIRST_RUNTIME_BLOCKER=_UIApplicationMain"
                              << "\nREASON=unsupported-delegate-method-metadata"
                              << "\nEXECUTION=stopped-at-unimplemented-import\n";
                    return 0;
                }
                std::cout << "APP_DELEGATE_METHOD_RESOLUTION=validated-local-guest-IMP\n";
                // Diagnostic callback of actual app-owned ARM64 IMP. No
                // UIKit app object, framework scheduler or window exists.
                // Zero UIApplication/options are valid ONLY for this
                // pinned fixture's inspected, scalar-return callback.
                anyios::darwin::GuestObjcObjectArena instances(
                    memory, kDiagnosticDelegate, 0x4000);
                const auto delegate = instances.allocate(
                    objc, *bridged_delegate_class);
                if (!delegate || memory.read(*delegate, 8) != *bridged_delegate_class) {
                    throw std::runtime_error(
                        "could not instantiate actual guest Objective-C delegate");
                }
                std::cout << "GUEST_DELEGATE_INSTANCE=allocated-from-class-ro"
                          << "\nGUEST_DELEGATE_ISA=original-guest-class\n";
                const std::array<std::uint64_t, 4> params{
                    *delegate, did_launch->selector, 0, 0
                };
                const auto result = anyios::abi::invoke_guest_callback(
                    *cpu, did_launch->entry, params, kReturn, 4096);
                std::cout << "APP_DELEGATE_GUEST_IMP=executed"
                          << "\nAPP_DELEGATE_METHOD=application:didFinishLaunchingWithOptions:"
                          << "\nAPP_DELEGATE_CALLBACK_RESULT=" << result
                          << "\nCALLBACK_SCOPE=diagnostic-only-no-UIKit-lifecycle\n";
                if (!instances.release(*delegate) || instances.is_live(*delegate)) {
                    throw std::runtime_error("guest delegate lifetime check failed");
                }
                std::cout << "GUEST_DELEGATE_RELEASE=verified\n";
                if (result != 1) {
                    throw std::runtime_error(
                        "original Objective-C app launch callback returned unexpected BOOL");
                }
                std::cout << "FIRST_RUNTIME_BLOCKER=_UIApplicationMain"
                          << "\nWINDOW=not-created"
                          << "\nEXECUTION=stopped-at-unimplemented-import\n";
                return 0;
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
