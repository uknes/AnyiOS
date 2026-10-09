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
void cursor_wrap_contract() {
    auto operand = [](std::vector<std::uint8_t>& code, std::uint64_t value) {
        do {
            auto byte = static_cast<std::uint8_t>(value & 0x7fU);
            value >>= 7;
            if (value) byte |= 0x80;
            code.push_back(byte);
        } while (value);
    };
    for (const bool combined : {false, true}) {
        std::vector<std::uint8_t> code{0x11,0x40,'_','f',0,0x71,32};
        if (combined) code.push_back(0xa0);
        else { code.push_back(0x90); code.push_back(0x80); }
        operand(code, UINT64_MAX-23); // Encoded backward delta -24.
        code.insert(code.end(), {0x90,0});
        auto bytes = file(code); const auto original = bytes;
        const auto decoded = anyios::dyld::inspect_legacy_eager_bind_stream(bytes,image(code.size()));
        check(decoded.sites.size()==2 && decoded.sites[0].file_offset==0x1020 &&
              decoded.sites[1].file_offset==0x1010 && bytes==original,
              "modular cursor lost bounded descending bind sites or changed source");
        check(decoded.cursor_wraps==1 && decoded.first_cursor_wrap &&
              decoded.first_cursor_wrap->before==(combined ? 32U : 40U) &&
              decoded.first_cursor_wrap->delta==UINT64_MAX-23 &&
              decoded.first_cursor_wrap->pointer_advance==(combined ? 8U : 0U) &&
              decoded.first_cursor_wrap->after==16,
              "cursor wrap diagnostic does not describe original opcode operands");
    }
    std::vector<std::uint8_t> repeated{0x11,0x40,'_','f',0,0x71,16,0xc0,3};
    operand(repeated, UINT64_MAX-15); // skip -16 plus pointer size gives -8.
    repeated.push_back(0);
    const auto sites = anyios::dyld::inspect_legacy_eager_bind_sites(file(repeated),image(repeated.size()));
    check(sites.size()==3 && sites[0].file_offset==0x1010 && sites[1].file_offset==0x1008 &&
          sites[2].file_offset==0x1000, "descending repeated binds lost or unused final cursor rejected");
    std::vector<std::uint8_t> invalid{0x11,0x40,'_','f',0,0x71,0,0x80};
    operand(invalid, UINT64_MAX-7);
    invalid.insert(invalid.end(),{0x90,0});
    fail(invalid); // Modular cursor must not turn an out-of-segment pointer into a binding.
    std::vector<std::uint8_t> absolute{0x11,0x40,'_','f',0,0x71,16,0x90,0};
    auto im=image(absolute.size()); im.segments[1].vm_address=UINT64_MAX-7;
    bool rejected=false;
    try { (void)anyios::dyld::inspect_legacy_eager_bind_sites(file(absolute),im); }
    catch (const anyios::macho::FormatError&) { rejected=true; }
    check(rejected,"absolute guest VM address overflow accepted");
}

void run(){
    const std::vector<std::uint8_t> code={
        0x11,0x40,'_','p','r','i','n','t','f',0x00,
        0x51,0x71,0x00,0x90,0xb1,0xa0,0x08,
        0xc0,0x02,0x08,0x00};
    auto data=file(code);
    auto im=image(code.size());
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
    const std::vector<std::uint8_t> signed_addend={
        0x11,0x41,'_','e','r','r',0x00,
        0x51,0x60,0x7e,0x71,0x00,0x90,0x00};
    const auto signed_found=anyios::dyld::inspect_legacy_eager_bind_sites(
        file(signed_addend),image(signed_addend.size()));
    check(signed_found.size()==1 && signed_found[0].addend==-2 &&
          signed_found[0].weak_import && signed_found[0].symbol=="_err",
          "negative SLEB addend or weak symbol flags lost");

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
    fail({0x12,0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x00}); // ordinal outside dylib list
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x00,0x80,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x02,0x90,0x00}); // overflow
    fail({0x11,0x40,'_','f',0x00,0x51,0x71,0x80}); // truncated address
    fail({0x11,0x51,0x71,0x00,0x90,0x00}); // missing symbol
    fail({0x40,'_','f',0x00,0x51,0x71,0x00,0x90,0x00}); // missing ordinal
    // Lazy records are independently addressable; DONE is not end-of-stream.
    const std::vector<std::uint8_t> lazy={
        0x11,0x40,'_','a',0,0x60,0x7e,0x71,0,0x90,0,
        0x11,0x41,'_','b',0,0x71,8,0x90,0,0,0};
    auto lazy_image=image(lazy.size());
    lazy_image.legacy_dyld->bind.reset();
    lazy_image.legacy_dyld->lazy_bind=anyios::macho::LinkeditRange{0x2800,static_cast<std::uint32_t>(lazy.size())};
    auto lazy_file=file(lazy); const auto before=lazy_file;
    const auto lazy_sites=anyios::dyld::inspect_legacy_lazy_bind_sites(lazy_file,lazy_image);
    check(lazy_sites.size()==2 && lazy_sites[0].addend==-2 &&
          lazy_sites[1].addend==0 && lazy_sites[1].weak_import &&
          lazy_sites[1].lazy_record_offset==11 && lazy_file==before,
          "lazy record state/offset or original input ownership lost");
    const std::vector<std::uint8_t> weak={
        0x48,'_','s',0, 0x40,'_','w',0,0x71,0,0x90,0};
    auto weak_image=image(weak.size());
    weak_image.legacy_dyld->bind.reset();
    weak_image.legacy_dyld->weak_bind=anyios::macho::LinkeditRange{0x2800,static_cast<std::uint32_t>(weak.size())};
    const auto weak_sites=anyios::dyld::inspect_legacy_weak_bind_sites(file(weak),weak_image);
    check(weak_sites.sites.size()==1 && weak_sites.sites[0].library_ordinal==-3 &&
          !weak_sites.sites[0].weak_import &&
          weak_sites.non_weak_definitions==std::vector<std::string>{"_s"},
          "weak lookup confused with weak import or strong definition discarded");
    auto bad_stream=[&](std::vector<std::uint8_t> code,bool is_lazy){
        auto im=image(code.size()); im.legacy_dyld->bind.reset();
        auto range=anyios::macho::LinkeditRange{0x2800,static_cast<std::uint32_t>(code.size())};
        if(is_lazy) im.legacy_dyld->lazy_bind=range; else im.legacy_dyld->weak_bind=range;
        bool rejected=false;
        try {
            if(is_lazy)(void)anyios::dyld::inspect_legacy_lazy_bind_sites(file(code),im);
            else (void)anyios::dyld::inspect_legacy_weak_bind_sites(file(code),im);
        }catch(const anyios::macho::FormatError&){rejected=true;}
        check(rejected,"malformed weak/lazy stream accepted");
    };
    bad_stream({0x11,0x40,'_','a',0,0x71,0,0x90},true); // missing record DONE
    bad_stream({0x11,0x40,'_','a',0,0x71,0,0x90,0,0x71,8,0x90,0},true); // inherited identity
    bad_stream({0x11,0x40,'_','a',0,0x71,0,0x90,0x90,0},true); // two binds
    bad_stream({0x11,0},true); // record with no bind
    bad_stream({0x51,0},true); // pointer type opcode forbidden by lazy grammar
    bad_stream({0x80,0,0},true); // lazy address arithmetic forbidden
    bad_stream({0x11,0},false); // ordinal forbidden in weak grammar
    bad_stream({0x44,'_','a',0,0},false); // reserved flag
    bad_stream({0x40,'_','a',0,0x71,0,0xc1,1,0,0},false); // reserved immediate
    bad_stream({0x40,'_','a',0,0x60,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x01,0},false); // SLEB overflow
    // Signed 64-bit endpoints are parsed without signed shifts/overflow.
    for (bool negative : {false,true}) {
        std::vector<std::uint8_t> endpoint={0x11,0x40,'_','e',0,0x60};
        endpoint.insert(endpoint.end(),9,negative ? 0x80 : 0xff);
        endpoint.push_back(negative ? 0x7f : 0x00);
        endpoint.insert(endpoint.end(),{0x71,0,0x90,0});
        const auto e=anyios::dyld::inspect_legacy_eager_bind_sites(file(endpoint),image(endpoint.size()));
        check(e[0].addend==(negative ? INT64_MIN : INT64_MAX),"SLEB endpoint lost");
    }
    const auto none=anyios::dyld::inspect_legacy_eager_bind_sites(data,anyios::macho::Image{});
    check(none.empty(),"nonlegacy image fabricated eager bind");
}
}
int main(){try{run();cursor_wrap_contract();std::cout<<"Bounded legacy eager bind opcode inspector passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
