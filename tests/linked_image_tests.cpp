#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Bytes = std::vector<std::byte>;
using anyios::cpu::GuestMemory;
using anyios::cpu::Access;

void u16(Bytes& b, std::size_t at, std::uint16_t v) {
    for (unsigned i = 0; i < 2; ++i) b.at(at + i) = std::byte((v >> (8 * i)) & 0xff);
}
void u32(Bytes& b, std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) b.at(at + i) = std::byte((v >> (8 * i)) & 0xff);
}
void u64(Bytes& b, std::size_t at, std::uint64_t v) {
    u32(b, at, static_cast<std::uint32_t>(v));
    u32(b, at + 4, static_cast<std::uint32_t>(v >> 32));
}
void check(bool good, std::string_view reason) {
    if (!good) throw std::runtime_error(std::string(reason));
}

Bytes fixture() {
    Bytes b(8192);
    u32(b, 0, 0xfeedfacf);
    u32(b, 4, 0x0100000c);
    u32(b, 12, 2);
    u32(b, 16, 5);
    u32(b, 20, 208);

    u32(b, 32, 0x19); u32(b, 36, 72);
    constexpr char text[] = "__TEXT";
    for (std::size_t i = 0; i < sizeof(text)-1; ++i) b.at(40+i) = std::byte(text[i]);
    u64(b, 32+24, 0x100000000ULL); u64(b, 32+32, 4096);
    u64(b, 32+40, 0); u64(b, 32+48, 4096);
    u32(b, 32+56, 5); u32(b, 32+60, 5);

    u32(b, 104, 0x19); u32(b, 108, 72);
    constexpr char data[] = "__DATA";
    for (std::size_t i = 0; i < sizeof(data)-1; ++i) b.at(112+i) = std::byte(data[i]);
    u64(b, 104+24, 0x100001000ULL); u64(b, 104+32, 4096);
    u64(b, 104+40, 4096); u64(b, 104+48, 4096);
    u32(b, 104+56, 3); u32(b, 104+60, 3);

    u32(b, 176, 0x80000028); u32(b, 180, 24); u64(b, 184, 0x300);
    u32(b, 200, 0x32); u32(b, 204, 24);
    u32(b, 208, 2);
    u32(b, 224, 0x80000034); u32(b, 228, 16);
    u32(b, 232, 256); u32(b, 236, 80);

    constexpr std::size_t payload = 256;
    u32(b, payload+4, 28); u32(b, payload+8, 64);
    u32(b, payload+12, 68); u32(b, payload+16, 1);
    u32(b, payload+20, 1);
    u32(b, payload+28, 2); u32(b, payload+36, 12);
    u32(b, payload+40, 24);
    u16(b, payload+44, 4096); u16(b, payload+46, 6);
    u64(b, payload+48, 4096);
    u16(b, payload+60, 1); u16(b, payload+62, 0);
    u32(b, payload+64, 1);
    b[payload+68] = std::byte{'f'};
    b[payload+69] = std::byte{'o'};
    b[payload+70] = std::byte{'o'};
    u64(b, 4096, (std::uint64_t{2} << 51) | 0x1234);
    u64(b, 4104, (std::uint64_t{1} << 63) | (std::uint64_t{3} << 24));
    u32(b, 0x300, 0xd2800540);
    u32(b, 0x304, 0xd65f03c0);
    return b;
}

void throws(const Bytes& source, GuestMemory& memory,
            std::uint64_t base,
            std::array<std::uint64_t, 1> imports,
            std::string_view error) {
    try {
        static_cast<void>(anyios::loader::stage_linked_image(source, memory, base, imports));
    } catch (const anyios::macho::FormatError& caught) {
        check(std::string_view(caught.what()).find(error) != std::string_view::npos, caught.what());
        return;
    }
    throw std::runtime_error("invalid linked image accepted");
}

void test_image_set() {
    auto main = fixture();
    auto library = fixture();
    u32(library, 12, 6);
    const auto main_original = main, library_original = library;
    const std::array<std::uint64_t, 1> to_library{0x20300}, to_main{0x10300};
    std::array<anyios::loader::LinkedImageInput, 2> inputs{{
        {main, 0x10000, to_library}, {library, 0x20000, to_main}
    }};
    GuestMemory memory(0x10000, 0x20000);
    const auto staged = anyios::loader::stage_linked_images(inputs, memory);
    check(staged.size() == 2 && staged[0].guest_entry == 0x10300 &&
          memory.read(0x11008, 8) == 0x20303 && memory.read(0x21008, 8) == 0x10303,
          "set lost cross-image bind targets/addends");
    check(main == main_original && library == library_original && !memory.write(0x10300, 0, 4) &&
          !memory.fetch(0x21000), "set modified input or lost page permissions");
    auto rejected = [&](GuestMemory& guest, std::span<const anyios::loader::LinkedImageInput> set,
                        std::string_view reason) {
        bool failed = false;
        try { (void)anyios::loader::stage_linked_images(set, guest); }
        catch (const anyios::macho::FormatError& error) {
            failed = std::string_view(error.what()).find(reason) != std::string_view::npos;
        }
        check(failed && !guest.fetch(0x10300) && !guest.read(0x11000, 1) && !guest.fetch(0x20300),
              "failed set leaked mappings or unexpected diagnostic");
    };
    GuestMemory guarded(0x10000, 0x20000);
    check(guarded.map(0x21000, 4096, 3) && guarded.write(0x21000, 0x5a, 1), "set guard setup");
    rejected(guarded, inputs, "linked guest mapping failed");
    check(guarded.read(0x21000, 1) == 0x5a, "rollback changed preexisting page");
    library[104+60] = std::byte{7};
    GuestMemory invalid(0x10000, 0x20000);
    rejected(invalid, inputs, "unsupported linked segment protections");
    library = library_original;
    const std::array<std::uint64_t, 1> outside{0x2f000}, addend_outside{0x21fff};
    inputs[0].resolved_imports = outside;
    GuestMemory foreign(0x10000, 0x20000);
    check(foreign.map(0x2f000, 4096, 5), "foreign target setup");
    rejected(foreign, inputs, "binding target outside image set");
    inputs[0].resolved_imports = addend_outside;
    GuestMemory addend(0x10000, 0x20000);
    rejected(addend, inputs, "binding target outside image set");
    inputs[0].resolved_imports = to_library;
    inputs[1].guest_base = 0x10000;
    GuestMemory overlap(0x10000, 0x20000);
    rejected(overlap, inputs, "readable segments overlap");
    inputs[1].guest_base = 0x20000;
    GuestMemory empty(0x10000, 0x20000);
    rejected(empty, {}, "invalid linked image set size");
    std::vector<anyios::loader::LinkedImageInput> huge(257, inputs[0]);
    rejected(empty, huge, "invalid linked image set size");
    const std::array<anyios::loader::LinkedImageInput, 1> no_main{inputs[1]};
    rejected(empty, no_main, "requires one executable");
}

void test_ios_mapping_policy() {
    anyios::macho::Image image;
    image.segments = {{"__TEXT", 0x10000, 0x8000, 0, 0x8000, 0, 5, 5},
                      {"__LINKEDIT", 0x18000, 0x150, 0x8000, 0x150, 0, 1, 1}};
    check(anyios::loader::ios_segment_mapping_size(image, 0) == 0x8000 &&
          anyios::loader::ios_segment_mapping_size(image, 1) == 0x4000,
          "native and guest LINKEDIT sizing policy differs");
    auto reject = [](const anyios::macho::Image& metadata, std::size_t index) {
        try { (void)anyios::loader::ios_segment_mapping_size(metadata, index); }
        catch (const anyios::macho::FormatError&) { return; }
        throw std::runtime_error("invalid iOS segment sizing accepted");
    };
    reject(image, 2);
    auto bad = image;
    bad.segments.back().init_protection = 3; reject(bad, 1);
    bad = image; bad.segments.back().init_protection = 5; reject(bad, 1);
    bad = image; bad.segments.back().name = "__DATA"; reject(bad, 1);
    bad = image; bad.segments.push_back(image.segments[0]); reject(bad, 1);
    bad = image; bad.segments.back().vm_address += 1; reject(bad, 1);
    bad = image; bad.segments.back().vm_size = 0; bad.segments.back().file_size = 0; reject(bad, 1);
    bad = image; bad.segments.back().file_size = 0x151; reject(bad, 1);
    bad = image; bad.segments.back().vm_size = UINT64_MAX; reject(bad, 1);
    bad = image; bad.segments.back().vm_address = UINT64_MAX - 0x3fff; reject(bad, 1);
    bad = image; bad.segments.back().vm_size = 64U * 1024U * 1024U + 1; reject(bad, 1);
}

void run() {
    const auto original = fixture();
    const std::array<std::uint64_t,1> imports{0x14000};
    GuestMemory memory(0x10000, 0x20000);
    const auto result = anyios::loader::stage_linked_image(original, memory, 0x10000, imports);
    check(result.guest_base == 0x10000 && result.guest_entry == 0x10300 &&
          result.segment_count == 2 && result.patched_pointers == 2, "linked image result");
    check(memory.fetch(0x10300) == 0xd2800540 &&
          memory.fetch(0x10304) == 0xd65f03c0, "linked executable instructions");
    check(memory.read(0x11000,8) == 0x11234, "rebase should use relocated guest base");
    check(memory.read(0x11008,8) == 0x14003, "bind target should preserve resolved guest address");
    check(!memory.write(0x10300, 0, 4), "executable page writable");
    check(!memory.fetch(0x11000), "data page executable");

    {
        auto legacy=fixture();
        // Replace the chained-fixups command with LC_DYLD_INFO_ONLY.
        for(std::size_t i=224;i<272;++i)legacy[i]=std::byte{0};
        u32(legacy,20,240); u32(legacy,224,0x80000022); u32(legacy,228,48);
        u32(legacy,232,0x400); u32(legacy,236,5);
        u32(legacy,240,0x420); u32(legacy,244,9);
        u32(legacy,256,0x440); u32(legacy,260,8);
        const std::array<unsigned char,5> rebase{0x11,0x21,0,0x53,0};
        const std::array<unsigned char,9> bind{0x10,0x40,'_','e',0,0x71,8,0x90,0};
        const std::array<unsigned char,8> lazy{0x10,0x40,'f',0,0x71,16,0x90,0};
        for(std::size_t i=0;i<rebase.size();++i)legacy[0x400+i]=std::byte{rebase[i]};
        for(std::size_t i=0;i<bind.size();++i)legacy[0x420+i]=std::byte{bind[i]};
        for(std::size_t i=0;i<lazy.size();++i)legacy[0x440+i]=std::byte{lazy[i]};
        u64(legacy,4096,0x100000300ULL);
        u64(legacy,4104,0x100000300ULL); u64(legacy,4112,0x100000300ULL);
        const auto untouched=legacy;
        const std::array<std::uint64_t,2> targets{0x14000,0x15000};
        GuestMemory mapped(0x10000,0x20000);
        const auto staged=anyios::loader::stage_linked_image(legacy,mapped,0x10000,targets,
            anyios::loader::LinkedImageOptions{false,nullptr,true});
        check(staged.patched_pointers==3 && mapped.read(0x11000,8)==0x10300 &&
              mapped.read(0x11008,8)==targets[0] && mapped.read(0x11010,8)==targets[1],
              "legacy rebase or eager/lazy prebind wrong");
        check(legacy==untouched && !mapped.write(0x10300,0,4) && !mapped.fetch(0x11000),
              "legacy source ownership or W xor X violated");
        auto rejected=[&](const Bytes& candidate,std::span<const std::uint64_t> addresses){
            GuestMemory empty(0x10000,0x20000); bool failed=false;
            try{(void)anyios::loader::stage_linked_image(candidate,empty,0x10000,addresses,
                anyios::loader::LinkedImageOptions{false,nullptr,true});}
            catch(const anyios::macho::FormatError&){failed=true;}
            check(failed && !empty.fetch(0x10300) && !empty.read(0x11000,1),
                  "failed legacy relocation leaked mapping");
        };
        rejected(legacy,std::span<const std::uint64_t>(targets).first(1));
        const std::array<std::uint64_t,2> unresolved{0x14000,0};rejected(legacy,unresolved);
        auto with_addend=[&](unsigned char delta){
            auto altered=legacy;
            const std::array<unsigned char,11> ops{0x10,0x40,'_','e',0,0x60,delta,0x71,8,0x90,0};
            for(std::size_t i=0;i<ops.size();++i)altered[0x420+i]=std::byte{ops[i]};
            u32(altered,244,static_cast<std::uint32_t>(ops.size()));return altered;
        };
        const auto minus_two=with_addend(0x7e);
        GuestMemory signed_memory(0x10000,0x20000);
        (void)anyios::loader::stage_linked_image(minus_two,signed_memory,0x10000,targets,
            anyios::loader::LinkedImageOptions{false,nullptr,true});
        check(signed_memory.read(0x11008,8)==0x13ffe,"legacy negative addend lost");
        const std::array<std::uint64_t,2> underflow{1,0x15000};rejected(minus_two,underflow);
        const std::array<std::uint64_t,2> overflow{UINT64_MAX,0x15000};rejected(with_addend(1),overflow);
        auto outside=legacy;u64(outside,4096,0xffffffffffffffffULL);rejected(outside,targets);
        auto duplicate=legacy;duplicate[0x446]=std::byte{0x90};
        duplicate[0x445]=std::byte{8};rejected(duplicate,targets);
        auto weak=legacy;u32(weak,248,0x460);u32(weak,252,1);rejected(weak,targets);
        GuestMemory occupied(0x10000,0x20000);
        check(occupied.map(0x11000,4096,3) && occupied.write(0x11000,0x5a,1),"legacy guard");
        bool rolled_back=false;
        try{(void)anyios::loader::stage_linked_image(legacy,occupied,0x10000,targets,
            anyios::loader::LinkedImageOptions{false,nullptr,true});}
        catch(const anyios::macho::FormatError&){rolled_back=true;}
        check(rolled_back && !occupied.fetch(0x10300) && occupied.read(0x11000,1)==0x5a,
              "legacy mapping journal failed to preserve occupied page");
    }
    {
        GuestMemory guarded(0x10000, 0x20000);
        check(guarded.map(0x11000, 4096, 3), "guard setup");
        throws(original, guarded, 0x10000, imports, "linked guest mapping failed");
        check(!guarded.fetch(0x10300).has_value(), "failed load leaked code mapping");
        check(guarded.write(0x11000, 0x11223344, 4), "failed load modified guard");
    }
    {
        GuestMemory strict(0x10000, 0x20000);
        try {
            (void)anyios::loader::stage_linked_image(
                original, strict, 0x11000, imports,
                anyios::loader::LinkedImageOptions{true, nullptr});
            throw std::runtime_error("unaligned iOS image accepted");
        } catch (const anyios::macho::FormatError& error) {
            check(std::string_view(error.what()).find("invalid guest linked-image configuration")
                      != std::string_view::npos, "guest base requires 16 KiB alignment");
        }
        check(!strict.fetch(0x11300), "misaligned iOS image leaked guest mapping");
        try {
            (void)anyios::loader::stage_linked_image(
                original, strict, 0x10000, imports,
                anyios::loader::LinkedImageOptions{true, nullptr});
            throw std::runtime_error("4 KiB fixture incorrectly accepted as 16 KiB iOS image");
        } catch (const anyios::macho::FormatError& error) {
            check(std::string_view(error.what()).find("16 KiB") != std::string_view::npos,
                  "strict iOS page validation did not reject 4 KiB segments");
        }
        check(!strict.fetch(0x10300), "rejected 4 KiB image leaked guest mapping");
    }
    {
        // Last read-only LINKEDIT may have a shorter declared VM extent;
        // the actual iOS page mapping must still cover 16 KiB.
        Bytes linked(32768);
        u32(linked, 0, 0xfeedfacf);
        u32(linked, 4, 0x0100000c);
        u32(linked, 12, 6);
        u32(linked, 16, 2);
        u32(linked, 20, 144);
        u32(linked, 32, 0x19); u32(linked, 36, 72);
        constexpr char text_name[] = "__TEXT";
        for (std::size_t i = 0; i < sizeof(text_name) - 1; ++i)
            linked[40+i] = std::byte(text_name[i]);
        u64(linked, 32+24, 0x100000000ULL);
        u64(linked, 32+32, 16384);
        u64(linked, 32+40, 0);
        u64(linked, 32+48, 16384);
        u32(linked, 32+56, 5);
        u32(linked, 32+60, 5);
        u32(linked, 104, 0x19); u32(linked, 108, 72);
        constexpr char linkedit_name[] = "__LINKEDIT";
        for (std::size_t i = 0; i < sizeof(linkedit_name) - 1; ++i)
            linked[112+i] = std::byte(linkedit_name[i]);
        u64(linked, 104+24, 0x100004000ULL);
        u64(linked, 104+32, 4096);
        u64(linked, 104+40, 16384);
        u64(linked, 104+48, 64);
        u32(linked, 104+56, 1);
        u32(linked, 104+60, 1);
        linked[16384] = std::byte{0x7f};
        GuestMemory strict(0x10000, 0x20000);
        const auto staged = anyios::loader::stage_linked_image(
            linked, strict, 0x10000, {},
            anyios::loader::LinkedImageOptions{true, nullptr});
        check(staged.segment_count == 2, "read-only LINKEDIT mapping omitted");
        check(strict.allowed(0x14000, 16384, Access::read) &&
              strict.read(0x14000, 1) == 0x7f &&
              strict.read(0x17fff, 1) == 0 &&
              !strict.fetch(0x14000),
              "LINKEDIT not rounded to one protected 16 KiB guest page");
        auto malformed = linked;
        u32(malformed, 104+60, 3);
        GuestMemory refused(0x10000, 0x20000);
        try {
            (void)anyios::loader::stage_linked_image(
                malformed, refused, 0x10000, {},
                anyios::loader::LinkedImageOptions{true, nullptr});
            throw std::runtime_error("writable 4 KiB LINKEDIT accepted");
        } catch (const anyios::macho::FormatError&) {
            check(!refused.allowed(0x14000, 4, Access::read),
                  "rejected LINKEDIT leaked guest mapping");
        }
    }
    {
        GuestMemory narrow(0x10000, 0x20000);
        throws(original, narrow, 0x2f000, imports, "linked guest mapping failed");
        check(!narrow.fetch(0x2f000), "failed load leaked partial mappings");
    }
    {
        auto bad = original;
        u16(bad, 256+46, 9);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "unsupported chained pointer format");
        check(!fresh.fetch(0x10300), "invalid fixup leaked mapping");
    }
    {
        auto bad = original;
        u32(bad, 104+60, 7);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "unsupported linked segment protections");
    }
    {
        auto bad = original;
        u32(bad, 224, 0x80000022);
        GuestMemory fresh(0x10000, 0x20000);
        // Replacing an 8-byte command with 48-byte LC_DYLD_INFO is malformed.
        // The new bounded parser must reject before any relocation or mapping.
        throws(bad, fresh, 0x10000, imports, "truncated LC_DYLD_INFO");
    }
    {
        GuestMemory fresh(0x10000, 0x20000);
        throws(original, fresh, 0x10000, {0}, "unresolved chained bind ordinal");
    }
    {
        auto bad = original;
        u64(bad, 4096, 0xffee);
        GuestMemory fresh(0x10000, 0x20000);
        throws(bad, fresh, 0x10000, imports, "linked rebase target outside image");
    }
}

}
int main() {
    try {
        run();
        test_image_set();
        test_ios_mapping_policy();
        std::cout << "Linked-image guest mapping and relocatable dyld patch tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "linked-image test failed: " << error.what() << '\n';
        return 1;
    }
}
