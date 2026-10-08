#include <anyios/guest_tls.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Fn>
void refuses(Fn&& call) {
    try { call(); }
    catch (const anyios::macho::FormatError&) { return; }
    throw std::runtime_error("unsupported TLV request was silently accepted");
}
}

int main() {
    try {
        using namespace anyios;
        cpu::GuestMemory memory(0x10000, 0x80000);
        const auto rw = cpu::bits(cpu::Access::read) |
                        cpu::bits(cpu::Access::write);
        const auto rx = cpu::bits(cpu::Access::read) |
                        cpu::bits(cpu::Access::execute);
        require(memory.map_ios(0x10000, 0x4000, rx) &&
                memory.map_ios(0x20000, 0x4000, rw),
                "owned TLV fixture mapping failed");
        // Synthetic instructions solely for validating the registered resolver PC.
        const std::byte return_instruction[] = {
            std::byte{0xc0}, std::byte{0x03}, std::byte{0x5f}, std::byte{0xd6}};
        require(memory.load(0x10000, return_instruction),
                "resolver stub mapping was not populated");
        require(memory.write(0x20000, 7, 4) &&
                memory.write(0x20100, 0x10000, 8) &&
                memory.write(0x20108, 0, 8) &&
                memory.write(0x20110, 0, 8) &&
                memory.write(0x20118, 0x10000, 8) &&
                memory.write(0x20120, 0, 8) &&
                memory.write(0x20128, 4, 8),
                "TLV fixture descriptors failed");
        macho::Image image{};
        macho::Segment text{};
        text.name = "__TEXT";
        text.vm_address = 0x1000;
        image.segments.push_back(text);
        macho::Section initialized{};
        initialized.name = "__thread_data";
        initialized.segment_name = "__DATA";
        initialized.address = 0x11000;
        initialized.size = 4;
        initialized.flags = 0x11;
        image.sections.push_back(initialized);
        macho::Section bss{};
        bss.name = "__thread_bss";
        bss.segment_name = "__DATA";
        bss.address = 0x11004;
        bss.size = 4;
        bss.zero_fill = true;
        bss.flags = 0x12;
        image.sections.push_back(bss);
        macho::Section descriptors{};
        descriptors.name = "__thread_vars";
        descriptors.segment_name = "__DATA";
        descriptors.address = 0x11100;
        descriptors.size = 48;
        descriptors.flags = 0x13;
        image.sections.push_back(descriptors);

        darwin::GuestTls tls(memory, 0x40000, 0x20000);
        tls.register_module(image, 0x10000);
        const auto h1 = tls.thread_register(1);
        const auto h2 = tls.thread_register(2);
        require(h1 != h2 && memory.read(h1, 8) == 1 &&
                memory.read(h2, 8) == 2, "TPIDRRO guest headers must be distinct");
        const auto a = tls.get_address(1, 0x20100);
        const auto b = tls.get_address(2, 0x20100);
        require(a != b && memory.read(a, 4) == 7 &&
                memory.read(b, 4) == 7, "TLV template or thread isolation failed");
        require(tls.get_address(1, 0x20100) == a,
                "TLV address changed within one guest thread");
        require(tls.get_address(1, 0x20118) == a + 4,
                "zero-filled TLV variable did not share module allocation");
        require(memory.read(a + 4, 4) == 0 &&
                memory.write(a, 19, 4) && memory.read(b, 4) == 7,
                "guest thread variable leaked to another guest thread");
        refuses([&] { (void)tls.get_address(1, 0x20101); });
        refuses([&] { (void)tls.get_address(3, 0x20100); });
        refuses([&] { tls.finish_thread(1); });
        auto invalid = image;
        invalid.sections.back().size = 25;
        refuses([&] {
            darwin::GuestTls test(memory, 0x60000, 0x4000);
            test.register_module(invalid, 0x10000);
        });
        require(memory.write(0x20110, 0x100000, 8),
                "corrupt TLV descriptor could not be injected");
        refuses([&] {
            darwin::GuestTls test(memory, 0x60000, 0x4000);
            test.register_module(image, 0x10000);
        });
        require(memory.write(0x20110, 0, 8),
                "corrupt TLV descriptor not restored");
        auto wrong_type = image;
        wrong_type.sections.back().flags = 0x11;
        refuses([&] {
            darwin::GuestTls test(memory, 0x60000, 0x4000);
            test.register_module(wrong_type, 0x10000);
        });
        std::cout << "Guest-only TLV descriptor, template, per-thread isolation and refusal tests passed\\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\\n';
        return 1;
    }
}
