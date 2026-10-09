#include <anyios/macho.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void put(std::vector<std::byte>& b, std::size_t p, std::uint32_t x) {
    for (unsigned i=0;i<4;++i)
        b[p+i] = std::byte{static_cast<unsigned char>((x>>(i*8))&255U)};
}
void expect(bool value, const char* why) { if(!value) throw std::runtime_error(why); }
void rejected(const std::vector<std::byte>& bytes) {
    bool threw=false;
    try { (void)anyios::macho::inspect(bytes); }
    catch (const anyios::macho::FormatError&) { threw=true; }
    expect(threw,"malformed legacy dyld metadata was accepted");
}
std::vector<std::byte> minimal() {
    std::vector<std::byte> data(160);
    put(data,0,0xfeedfacf);
    put(data,4,0x0100000c); // arm64
    put(data,12,2); // MH_EXECUTE
    put(data,16,1); put(data,20,48);
    put(data,32,0x80000022); put(data,36,48);
    put(data,40,120); put(data,44,4); // rebase
    put(data,48,128); put(data,52,8); // bind
    put(data,64,136); put(data,68,3); // lazy bind
    return data;
}
void run() {
    auto data=minimal();
    auto info=anyios::macho::inspect(data);
    expect(info.legacy_dyld && info.legacy_dyld->rebase &&
           info.legacy_dyld->rebase->file_offset==120 &&
           info.legacy_dyld->bind->file_size==8 &&
           info.legacy_dyld->lazy_bind->file_size==3 &&
           !info.legacy_dyld->weak_bind && !info.has_chained_fixups,
           "valid legacy dyld linkedit ranges lost");
    auto bad=data;put(bad,40,158);put(bad,44,8);rejected(bad);
    bad=data;put(bad,44,16U*1024U*1024U+1);rejected(bad);
    bad=data;put(bad,36,40);rejected(bad);
    bad=data;put(bad,40,0);rejected(bad);
    bad=data;put(bad,20,64);put(bad,16,2);
    put(bad,80,5);put(bad,84,16); // LC_UNIXTHREAD
    info=anyios::macho::inspect(bad);
    expect(info.has_unixthread && info.legacy_dyld,
           "legacy thread metadata not recognized");
    bad=data;put(bad,20,96);put(bad,16,2);
    put(bad,80,0x22);put(bad,84,48);rejected(bad);
    auto exported=data;
    exported.resize(200);
    put(exported,72,144);put(exported,76,15);
    const std::uint8_t trie[]{0,1,'_','v','a','l','u','e',0,10,3,0,0x80,2,0};
    for(std::size_t i=0;i<sizeof(trie);++i)exported[144+i]=std::byte{trie[i]};
    info=anyios::macho::inspect(exported);
    expect(info.has_export_trie && info.exports.size()==1 &&
           info.exports[0].name=="_value" && info.exports[0].address==256,
           "legacy export trie was not decoded");
    bad=exported;bad[144+9]=std::byte{0};rejected(bad);
    bad=exported;put(bad,76,14);rejected(bad);
    auto aliased=exported;put(aliased,20,64);put(aliased,16,2);
    put(aliased,80,0x80000033);put(aliased,84,16);
    put(aliased,88,144);put(aliased,92,15);
    expect(anyios::macho::inspect(aliased).exports.size()==1,
           "identical legacy and modern export range rejected");
    bad=aliased;put(bad,88,160);rejected(bad);
    bad=aliased;put(bad,92,16);rejected(bad);
}
}
int main() {
    try { run(); std::cout << "Bounded legacy dyld/thread Mach-O metadata passed\n"; return 0; }
    catch(const std::exception& err){std::cerr<<err.what()<<'\n';return 1;}
}
