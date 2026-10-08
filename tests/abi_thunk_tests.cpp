#include <anyios/abi_thunk.hpp>

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
using namespace anyios::abi;
void expect(bool valid, std::string_view message) {
    if (!valid) throw std::runtime_error(std::string(message));
}
void denies(const std::function<void()>& f, std::string_view message) {
    try { f(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(std::string(message));
}
}

int main() {
    try {
        anyios::cpu::CpuState guest{};
        guest.x[0] = 0xffffffffU;
        guest.x[1] = 0x1234ffffU;
        guest.x[2] = 0x80000000U;
        guest.x[3] = 41;
        guest.x[18] = 0xdeadbeef1234ULL;
        guest.pc = 0x10000;
        guest.sp = 0x20000;
        const FixedSignature signature{{ScalarKind::signed8,
                                        ScalarKind::unsigned16,
                                        ScalarKind::signed32,
                                        ScalarKind::unsigned64}};
        int invoked = 0;
        const HostFixedFunction callback = [&](std::span<const std::uint64_t> args) {
            ++invoked;
            expect(args.size() == 4, "host signature argument count");
            expect(args[0] == UINT64_MAX, "signed8 must sign-extend from low byte");
            expect(args[1] == 65535, "unsigned16 must zero-extend from low 16 bits");
            expect(args[2] == 0xffffffff80000000ULL, "signed32 must sign-extend");
            expect(args[3] == 41, "unsigned64 must preserve all bits");
            return std::uint64_t{42};
        };
        expect(call_fixed_host_function(guest, signature, callback) == 42,
               "typed callback did not execute or return result");
        expect(invoked == 1, "host function was not called exactly once");
        expect(guest.x[18] == 0xdeadbeef1234ULL &&
               guest.pc == 0x10000 && guest.sp == 0x20000,
               "ABI marshaler mutated platform register or guest context");

        denies([&] { (void)decode_fixed_arguments(guest, {{ScalarKind::pointer}}); },
               "raw guest pointer accepted");
        denies([&] { (void)decode_fixed_arguments(guest, {{ScalarKind::aggregate}}); },
               "unsupported aggregate accepted");
        denies([&] { (void)decode_fixed_arguments(guest, {{ScalarKind::signed8}, true}); },
               "variadic Apple calling convention accepted");
        denies([&] {
            (void)decode_fixed_arguments(guest,
               {{ScalarKind::unsigned8, ScalarKind::unsigned8, ScalarKind::unsigned8,
                 ScalarKind::unsigned8, ScalarKind::unsigned8, ScalarKind::unsigned8,
                 ScalarKind::unsigned8, ScalarKind::unsigned8, ScalarKind::unsigned8}});
        }, "stack argument silently accepted");
        denies([&] {
            (void)call_fixed_host_function(guest, signature, {});
        }, "missing host thunk implementation accepted");
        expect(invoked == 1, "unsupported call accidentally invoked host callback");
        std::cout << "Typed Apple ARM64 guest scalar ABI thunk tests passed\\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\\n';
        return 1;
    }
}
