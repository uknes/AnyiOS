#include <anyios/legacy_rebase.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <span>
#include <string_view>
namespace {
using Bytes=std::vector<std::byte>;
void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}
Bytes sample(const std::vector<std::uint8_t>& instructions) {
    Bytes b(0x3000);
    for(std::size_t i=0;i<instructions.size();++i)
        b.at(0x2800+i)=std::byte{instructions[i]};
    return b;
}
anyios::macho::Image image(std::size_t count) {
    anyios::macho::Image im;
    im.segments.push_back({"__TEXT",0x100000000ULL,0x1000,0,0x1000,0,5,5});
    im.segments.push_back({"__DATA",0x100001000ULL,0x1000,0x1000,0x1000,0,3,3});
    anyios::macho::LegacyDyldInfo d;
    d.rebase=anyios::macho::LinkeditRange{0x2800,static_cast<std::uint32_t>(count)};
    im.legacy_dyld=d;
    return im;
}
void invalid(const std::vector<std::uint8_t>& code) {
    auto im=image(code.size());
    auto bytes=sample(code);
    bool failed=false;
    try { (void)anyios::dyld::inspect_legacy_rebase_sites(bytes,im); }
    catch(const anyios::macho::FormatError&){failed=true;}
    check(failed,"unsafe or malformed dyld rebase bytecode was accepted");
}
void run(){
    const std::vector<std::uint8_t> code{
        0x11,0x21,0x08,0x52,0x30,0x08,0x71,0x10,0x80,0x02,0x08,0x00};
    auto bytes=sample(code);auto im=image(code.size());
    const auto found=anyios::dyld::inspect_legacy_rebase_sites(bytes,im);
    constexpr std::array<std::uint64_t,5> expected{
        0x1008,0x1010,0x1020,0x1038,0x1048};
    check(found.size()==expected.size(),"wrong legacy rebases count");
    for(std::size_t i=0;i<expected.size();++i)
        check(found[i].file_offset==expected[i] &&
              found[i].vm_address==0x100000000ULL+expected[i],
              "legacy rebase pointer target not original segment offset");
    check(std::to_integer<std::uint8_t>(bytes[0x2800])==0x11,
          "read-only rebase inspection modified original Mach-O");
    invalid({0x11,0x21,0x00,0x52}); // no DONE
    invalid({0x12,0x21,0x00,0x51,0x00}); // text rebase width unsupported
    invalid({0x11,0x22,0x00,0x51,0x00}); // missing segment
    invalid({0x11,0x51,0x00}); // no selected segment
    invalid({0x11,0x20,0x00,0x51,0x00}); // pointer in read+execute __TEXT
    invalid({0x11,0x21,0x80}); // truncated ULEB
    invalid({0x11,0x21,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x02,0x00});
    invalid({0x11,0x21,0x00,0x60,0xff,0xff,0x7f,0x00}); // excessive count
    invalid({0x11,0x21,0x00,0x51,0x21,0x00,0x51,0x00}); // duplicate original pointer
    invalid({0x11,0x21,0x01,0x51,0x00}); // unaligned pointer
    invalid({0x11,0x21,0x80,0x20,0x51,0x00}); // beyond segment
    invalid({0x11,0x21,0x00,0x51,0x00,0x51}); // trailing nonzero opcode after DONE
    invalid({0x11,0x21,0x00,0x91,0x00}); // unknown opcode
    const auto none=anyios::dyld::inspect_legacy_rebase_sites(bytes,anyios::macho::Image{});
    check(none.empty(),"nonlegacy image fabricated rebases");
}
}
int main(){try{run();std::cout<<"Bounded read-only legacy dyld rebase opcode tests passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
