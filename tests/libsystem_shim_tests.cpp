#include <anyios/libsystem_shim.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool yes, std::string_view what) {
    if (!yes) throw std::runtime_error(std::string(what));
}
}
int main() {
    try {
        anyios::cpu::GuestMemory memory(0x10000, 0x80000);
        anyios::darwin::LibSystemShim lib(memory, 0x20000, 0x10000);
        const auto first = lib.invoke("_malloc", {8, 0, 0});
        const auto second = lib.invoke("_malloc", {17, 0, 0});
        require(first.value == 0x20000 && second.value == 0x20010,
                "bump allocator alignment");
        require(memory.allowed(first.value, 16, anyios::cpu::Access::write),
                "returned guest pointer must be writable");
        require(memory.write(first.value, 'O', 1) &&
                memory.write(first.value + 1, 'K', 1), "prepare owned buffer");
        const auto written = lib.invoke("_write", {1, first.value, 2});
        require(written.value == 2 && lib.output() == "OK", "captured guest stdout");
        require(lib.invoke("_exit", {23, 0, 0}).exited, "guest exit not signaled");
        require(lib.invoke("_malloc", {1024 * 1024, 0, 0}).value == 0,
                "heap overcommit must return null guest pointer");
        require(lib.invoke("_malloc", {16, 0, 0}).value != 0,
                "heap failed after oversized allocation");
        require(!memory.fetch(first.value), "heap memory became executable");
        try {
            (void)lib.invoke("_unknown", {0, 0, 0});
            throw std::runtime_error("missing libSystem symbol was accepted");
        } catch (const std::invalid_argument&) { }
        try {
            (void)lib.invoke("_write", {99, first.value, 2});
            throw std::runtime_error("invalid host fd silently accepted");
        } catch (const std::runtime_error& e) {
            if (std::string_view(e.what()).find("unsupported Darwin") == std::string_view::npos)
                throw;
        }
        require(lib.output() == "OK", "failed write changed guest output");
        const auto a = lib.invoke("_malloc", {64, 0, 0}).value;
        const auto b = lib.invoke("_malloc", {64, 0, 0}).value;
        require(a && b && memory.write(a, 'a', 1) && memory.write(a + 1, 0, 1)
                && memory.write(b, 'b', 1) && memory.write(b + 1, 0, 1),
                "string fixture initialization");
        require(lib.invoke("_strlen", {a, 0, 0}).value == 1, "strlen ABI");
        require(lib.invoke("_strcmp", {a, b, 0}).value == 0xffffffffULL,
                "negative narrow-int strcmp must return w0");
        require(lib.invoke("_strcmp", {b, a, 0}).value == 1, "positive strcmp");
        require(lib.invoke("_strcmp", {a, a, 0}).value == 0, "equal strcmp");
        require(lib.invoke("_memset", {b, 0x123456ff00ULL, 2}).value == b &&
                memory.read(b, 1) == 0 && memory.read(b + 1, 1) == 0,
                "memset must truncate int to unsigned char");
        require(lib.invoke("_memcpy", {b, a, 2}).value == b &&
                memory.read(b, 1) == 'a' && memory.read(b + 1, 1) == 0,
                "memcpy buffer and guest pointer result");
        require(lib.invoke("_memcpy", {0, 0, 0}).value == 0,
                "zero-length memcpy must not access guest memory");
        require(lib.invoke("_strlen", {b, 0, 0}).value == 1,
                "copied string is not readable");
        try {
            (void)lib.invoke("_memcpy", {a + 1, a, 4});
            throw std::runtime_error("overlapping memcpy silently succeeded");
        } catch (const std::runtime_error& error) {
            if (std::string_view(error.what()).find("overlapping") ==
                std::string_view::npos) throw;
        }
        try {
            (void)lib.invoke("_memset", {b, 0, 65537});
            throw std::runtime_error("oversize memset silently succeeded");
        } catch (const std::runtime_error& error) {
            if (std::string_view(error.what()).find("unsupported libSystem") ==
                std::string_view::npos) throw;
        }
        try {
            (void)lib.invoke("_memcpy", {0x10000, a, 2});
            throw std::runtime_error("unmapped guest memcpy allowed");
        } catch (const std::runtime_error& error) {
            if (std::string_view(error.what()).find("unsupported libSystem") ==
                std::string_view::npos) throw;
        }
        require(memory.read(a, 1) == 'a', "failed call mutated guest source");
        // Validate the entire range before writing even its first byte.
        const auto guarded = 0x80000ULL;
        require(memory.map(guarded, 4096, 3) && memory.write(guarded+4095, 0x5a, 1),
                "boundary destination fixture");
        auto rejects_copy=[&](std::uint64_t dst,std::uint64_t src,std::uint64_t size){
            bool rejected=false;
            try{(void)lib.invoke("_memcpy",{dst,src,size});}
            catch(const std::runtime_error&){rejected=true;}
            require(rejected,"unsafe guest copy accepted");
        };
        rejects_copy(guarded+4095,a,2);
        require(memory.read(guarded+4095,1)==0x5a,"partial destination changed on failure");
        require(memory.write(b,0x5a,1),"source failure fixture");
        rejects_copy(b,UINT64_MAX,2);
        rejects_copy(b,0x90000,2);
        require(memory.read(b,1)==0x5a,"unreadable source changed destination");
        require(memory.map(0x82000,4096,1),"readonly destination fixture");
        rejects_copy(0x82000,a,1);
        rejects_copy(a,a,1);
        rejects_copy(b,a,UINT64_MAX);
        std::cout << "Minimal host libSystem _malloc/_write/_exit contract passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
