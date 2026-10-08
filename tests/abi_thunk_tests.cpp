#include <anyios/abi_thunk.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
class MutatingBackend final : public anyios::cpu::CpuBackend {
public:
    explicit MutatingBackend(unsigned changed_register) : changed_(changed_register) {}
    anyios::cpu::CpuState state() const override { return guest_; }
    void set_state(const anyios::cpu::CpuState& s) override { guest_ = s; }
    anyios::cpu::CpuEvent step() override {
        guest_.x[changed_] ^= 0x1;
        guest_.pc = guest_.x[30];
        return {};
    }
private:
    unsigned changed_;
    anyios::cpu::CpuState guest_{};
};
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

        {
            using anyios::cpu::Access;
            using anyios::cpu::GuestMemory;
            GuestMemory memory(0x10000, 0x5000);
            expect(memory.map_ios(0x10000, 16384, anyios::cpu::bits(Access::read) |
                anyios::cpu::bits(Access::write)), "guest vararg stack map");
            guest.sp = 0x10080;
            guest.x[0] = 7;
            guest.x[1] = 0xabcddcba11223344ULL;
            guest.x[2] = 0x55667788ULL;
            expect(memory.write(guest.sp, 0xffffffffU, 8), "write signed variadic");
            expect(memory.write(guest.sp + 8, 41, 8), "write unsigned variadic");
            const FixedSignature variadic{{ScalarKind::signed32}, true};
            const std::array<ScalarKind, 2> kinds{
                ScalarKind::signed32, ScalarKind::unsigned64
            };
            const auto vals = decode_apple_variadic_arguments(guest, memory, variadic, kinds);
            expect(vals.size() == 3 && vals[0] == 7 &&
                   vals[1] == UINT64_MAX && vals[2] == 41,
                   "Apple variadics must read stack slots, not x1/x2 registers");
            const auto v = call_variadic_host_function(guest, memory, variadic, kinds,
                [](std::span<const std::uint64_t> args) {
                    return args[0] + args[2];
                });
            expect(v == 48, "variadic typed adapter returned wrong value");
            const std::array<ScalarKind, 1> bad{ScalarKind::pointer};
            denies([&] { (void)decode_apple_variadic_arguments(guest, memory, variadic, bad); },
                "unsafe pointer variadic accepted");
            guest.sp += 8;
            denies([&] { (void)decode_apple_variadic_arguments(guest, memory, variadic, kinds); },
                "misaligned Apple variadic stack accepted");
            guest.sp = 0x10080;
            const auto original = guest;
            expect(guest.x[18] == original.x[18], "variadic adapter changed guest x18");
            guest.sp = 0x20000;
        }

        for (const auto register_id : {18u, 19u, 29u}) {
            MutatingBackend backend(register_id);
            anyios::cpu::CpuState saved{};
            saved.pc = 0x10000;
            saved.sp = 0x20000;
            saved.x[18] = 0xaa;
            saved.x[19] = 0xbb;
            saved.x[29] = 0xcc;
            backend.set_state(saved);
            const std::array<std::uint64_t, 1> input{42};
            try {
                (void)invoke_guest_callback(backend, 0x11000, input, 0x12000, 4);
                throw std::runtime_error("guest register clobber was accepted");
            } catch (const std::runtime_error& error) {
                if (std::string_view(error.what()).find("clobbered") == std::string_view::npos)
                    throw;
            }
            expect(backend.state().pc == saved.pc &&
                   backend.state().x[register_id] == saved.x[register_id],
                   "failing guest callback did not restore original state");
        }

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
