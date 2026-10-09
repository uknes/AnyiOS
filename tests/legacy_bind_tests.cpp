#include <anyios/legacy_bind.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Bytes=std::vector<std::byte>;
void check(bool value,const char* what){if(!value)throw std::runtime_error(what);}
anyios::macho::Image image(std::size_t count) {
    anyios::macho::Image im;
    im.segments.push_back({"__TEXT",0x100000000ULL,0x1000,0,0x1000,0,5,5});
    im.segments.push_back({"__DATA",0x100001000ULL,0x1000,0x1000,0x1000,0,3,3});
    im.dependencies.push_back({"/usr/lib/libSystem.B.dylib",false,false,false});
    anyios::macho::LegacyDyldInfo d;
    d.bind=anyios::macho::LinkeditRange{0x2800,static_cast<std::uint32_t>(count)};
    im.legacy_dyld=d; return im;
}
Bytes file(const std::vector<std::uint8_t>& code) {
    Bytes f(0x3000);
    for(std::size_t i=0;i<code.size();++i) f.at(0x2800+i)=std::byte{code[i]};
    return f;
}
void fail(const std::vector<std::uint8_t>& code) {
    auto im=image(code.size());auto original=file(code);bool rejected=false;
    try{(void)anyios::dyld::inspect_legacy_eager_bind_sites(original,im);}
    catch(const anyios::macho::FormatError&){rejected=true;}
    check(rejected,"invalid legacy bind opcode sequence was accepted");
}
void run(){
    const std::vector<std::uint8_t> code={
        0x11,0x40,'_','p','r','i','n','t','f',0x00,
        0x51,0x71,0x00,0x90,0xb1,0xa0,0x08,
        0xc0,0x02,0x08,0x00};
    auto data=file(code),im=image(code.size());
    const auto found=anyios::dyld::inspect_legacy_eager_bind_sites(data,im);
    const std::vector<std::uint64_t> expected={0x1000,0x1008,0x1018,0x1028,0x1038};
    check(found.size()==expected.size(),"wrong number of legacy eager binds");
    for(std::size_t i=0;i<expected.size();++i){
        check(found[i].file_offset==expected[i] &&
              found[i].vm_address==0x100000000ULL+expected[i] &&
              found[i].symbol=="_printf" && found[i].library_ordinal==1 &&
              found[i].addend==0 && !found[i].weak_import,
              "wrong bounded legacy bind address or symbol/ordinal");
    }
    check(std::to_integer<std::uint8_t>(data[0x2800])==0x11,
          "read-only inspection modified original Mach-O");
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x90}); // no DONE
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x80}); // truncated ULEB
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x00,0x90}); // suffix
    fail({0x11,0x40,'_','f',0x00,0x51,0x70,0x00,0x90,0x00}); // executable target
    fail({0x11,0x40,'_','f',0x00,0x51,0x72,0x00,0x90,0x00}); // bad segment
    fail({0x11,0x40,'_','f',0x00,0x52,0x71,0x00,0x90,0x00}); // type
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x01,0x90,0x00}); // unaligned
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x71,0x00,0x90,0x00}); // duplicate
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0xc0,0xff,0xff,0x7f,0x08,0x00}); // cap
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0xd0,0x00}); // threaded
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x00,0x02}); // trailing
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x80,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x02,0x90,0x00}); // overflow
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x80}); // truncated address
    fail({0x11,0x51,0x71,0x00,0x90,0x00}); // missing symbol
    fail({0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x00}); // missing ordinal
    const auto none=anyios::dyld::inspect_legacy_eager_bind_sites(data,anyios::macho::Image{});
    check(none.empty(),"nonlegacy image fabricated eager bind");
}
}
int main(){try{run();std::cout<<"Bounded legacy eager bind opcode inspector passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
