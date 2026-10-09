#include <anyios/legacy_bind.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>
namespace anyios::dyld {
namespace {
constexpr std::size_t kMaxSites = 65536;
std::uint64_t add(std::uint64_t x, std::uint64_t y) {
    if (y > UINT64_MAX - x) throw macho::FormatError("legacy bind address overflow");
    return x + y;
}
std::uint64_t uleb(std::span<const std::byte> data,std::size_t& p) {
    std::uint64_t value=0;
    for (unsigned i=0;i<10;++i) {
        if (p>=data.size()) throw macho::FormatError("truncated legacy bind ULEB");
        const auto c=std::to_integer<std::uint8_t>(data[p++]);
        if (i==9 && c>1) throw macho::FormatError("legacy bind ULEB overflow");
        value |= std::uint64_t(c&0x7fU)<<(7*i);
        if (!(c&0x80U)) return value;
    }
    throw macho::FormatError("unterminated legacy bind ULEB");
}
std::int64_t sleb(std::span<const std::byte> data,std::size_t& p) {
    std::uint64_t value=0;
    for (unsigned i=0;i<10;++i) {
        if (p>=data.size()) throw macho::FormatError("truncated legacy bind SLEB");
        const auto c=std::to_integer<std::uint8_t>(data[p++]);
        if (i==9 && c!=0x00 && c!=0x7f) {
            throw macho::FormatError("legacy bind SLEB overflow");
        }
        value |= std::uint64_t(c&0x7fU)<<(7*i);
        if (!(c&0x80U)) {
            if (i<9 && (c&0x40U)) value |= (UINT64_MAX << (7*(i+1)));
            return std::bit_cast<std::int64_t>(value);
        }
    }
    throw macho::FormatError("unterminated legacy bind SLEB");
}
}
namespace {
enum class StreamKind { eager, weak, lazy };
LegacyBindStream inspect_bind_stream(std::span<const std::byte> file,
    const macho::Image& image, StreamKind kind) {
    if (!image.legacy_dyld) return {};
    const auto selected = kind == StreamKind::eager ? image.legacy_dyld->bind :
        kind == StreamKind::weak ? image.legacy_dyld->weak_bind : image.legacy_dyld->lazy_bind;
    if (!selected || selected->file_size == 0) return {};
    const auto range=*selected;
    if(range.file_offset>file.size() || range.file_size>file.size()-range.file_offset ||
       range.file_size>16U*1024U*1024U)
        throw macho::FormatError("legacy bind stream out of bounds");
    const auto stream=file.subspan(range.file_offset,range.file_size);
    LegacyBindStream result;
    auto& sites = result.sites;
    std::unordered_set<std::uint64_t> visited;
    bool ordinal_set=kind==StreamKind::weak,symbol_set=false,segment_set=false;
    bool weak=false,done=false;
    std::int32_t ordinal=kind==StreamKind::weak ? -3 : 0;
    std::int64_t addend=0;
    std::uint32_t seg_index=0;
    std::uint64_t segment_offset=0;
    std::string symbol;
    std::size_t p=0;
    bool record_open=false;
    bool record_bound=false;
    std::uint64_t record_offset=0;
    auto advance=[&](std::uint64_t d){segment_offset=add(segment_offset,d);};
    auto emit=[&](){
        if (kind==StreamKind::lazy && record_bound)
            throw macho::FormatError("multiple bindings in one lazy record");
        if(!ordinal_set || !symbol_set || !segment_set)
            throw macho::FormatError("legacy bind missing ordinal, symbol, or segment");
        if(ordinal>0 && static_cast<std::size_t>(ordinal)>image.dependencies.size())
            throw macho::FormatError("legacy bind library ordinal outside dependencies");
        if(seg_index>=image.segments.size())
            throw macho::FormatError("legacy bind segment index invalid");
        const auto& segment=image.segments[seg_index];
        if((segment_offset&7U) || segment_offset>segment.file_size ||
           segment.file_size-segment_offset<8 ||
           segment_offset>segment.vm_size || segment.vm_size-segment_offset<8 ||
           (segment.init_protection&3U)!=3U || (segment.init_protection&4U))
            throw macho::FormatError("legacy bind pointer outside writable file-backed segment");
        const auto file_at=add(segment.file_offset,segment_offset);
        if(file_at>file.size() || file.size()-file_at<8)
            throw macho::FormatError("legacy bind pointer beyond original Mach-O");
        if(sites.size()>=kMaxSites)
            throw macho::FormatError("legacy bind site cap exceeded");
        if(!visited.insert(file_at).second)
            throw macho::FormatError("duplicate legacy bind site");
        record_bound=true;
        sites.push_back({file_at,add(segment.vm_address,segment_offset),symbol,ordinal,addend,weak,record_offset});
    };
    while(p<stream.size()) {
        const auto byte=std::to_integer<std::uint8_t>(stream[p++]);
        const auto cmd=byte&0xf0U,imm=byte&0x0fU;
        if (kind==StreamKind::weak && (cmd==0x10 || cmd==0x20 || cmd==0x30))
            throw macho::FormatError("dylib ordinal forbidden in weak bind stream");
        if (kind==StreamKind::lazy && (cmd==0x50 || cmd==0x80 || cmd==0xa0 || cmd==0xb0 || cmd==0xc0))
            throw macho::FormatError("opcode forbidden in lazy bind stream");
        if (kind==StreamKind::lazy && cmd!=0 && !record_open) {
            record_offset=p-1;
            record_open=true;
        }
        switch(cmd){
            case 0x00:
                if(imm)throw macho::FormatError("bad legacy bind DONE");
                if (kind==StreamKind::lazy) {
                    if (record_open && !record_bound)
                        throw macho::FormatError("lazy record has no binding");
                    // Each independently addressed lazy record starts with fresh
                    // state. Never inherit an ordinal or addend from another record.
                    ordinal_set=symbol_set=segment_set=false;
                    ordinal=0; addend=0; weak=false; symbol.clear();
                    record_open=record_bound=false; done=true;
                    break;
                }
                done=true;
                while(p<stream.size())
                    if(stream[p++]!=std::byte{0})
                        throw macho::FormatError("nonzero trailing legacy bind bytes");
                break;
            case 0x10:
                ordinal=imm;ordinal_set=true;break;
            case 0x20: {
                const auto value=uleb(stream,p);
                if(value>INT32_MAX)throw macho::FormatError("legacy bind ordinal overflow");
                ordinal=static_cast<std::int32_t>(value);ordinal_set=true;break;
            }
            case 0x30:
                ordinal=imm==0?0:static_cast<std::int32_t>(imm)-16;
                if(ordinal< -3)throw macho::FormatError("unknown special dyld bind ordinal");
                ordinal_set=true;break;
            case 0x40: {
                if(imm & ~(kind==StreamKind::weak ? 9U : 1U))throw macho::FormatError("unsupported legacy bind symbol flags");
                weak=(imm&1U)!=0;symbol.clear();
                bool end=false;
                for(std::size_t i=0;i<255 && p<stream.size();++i) {
                    const auto c=std::to_integer<std::uint8_t>(stream[p++]);
                    if(c==0){end=true;break;}
                    if(c<33 || c>126)throw macho::FormatError("invalid legacy bind symbol");
                    symbol.push_back(static_cast<char>(c));
                }
                if(!end || symbol.empty())
                    throw macho::FormatError("unterminated legacy bind symbol");
                if (kind==StreamKind::weak && (imm&8U)) {
                    if (result.non_weak_definitions.size()>=kMaxSites)
                        throw macho::FormatError("legacy weak definition cap exceeded");
                    result.non_weak_definitions.push_back(symbol);
                }
                symbol_set=true;break;
            }
            case 0x50:
                if(imm!=1)throw macho::FormatError("unsupported legacy bind pointer kind");
                break;
            case 0x60: addend=sleb(stream,p);break;
            case 0x70:
                seg_index=imm;segment_offset=uleb(stream,p);
                if(seg_index>=image.segments.size())
                    throw macho::FormatError("invalid legacy bind segment");
                segment_set=true;break;
            case 0x80: advance(uleb(stream,p));break;
            case 0x90:
                if(imm)throw macho::FormatError("invalid legacy DO_BIND immediate");
                emit();advance(8);break;
            case 0xa0:
                if(imm)throw macho::FormatError("invalid legacy bind skip immediate");
                {auto skip=uleb(stream,p);emit();advance(add(8,skip));}break;
            case 0xb0:
                emit();advance(8U+std::uint64_t(imm)*8U);break;
            case 0xc0: {
                if(imm)throw macho::FormatError("invalid legacy bind repeat immediate");
                const auto count=uleb(stream,p),skip=uleb(stream,p);
                if(count>kMaxSites-sites.size())
                    throw macho::FormatError("legacy bind count cap exceeded");
                const auto stride=add(8,skip);
                for(std::uint64_t i=0;i<count;++i){emit();advance(stride);}
                break;
            }
            default:
                throw macho::FormatError("unsupported legacy bind opcode or threaded binds");
        }
        if(done && kind!=StreamKind::lazy)break;
    }
    if(!done || record_open)throw macho::FormatError("legacy bind stream missing DONE");
    return result;
}
} // namespace
std::vector<LegacyBindSite> inspect_legacy_eager_bind_sites(
    std::span<const std::byte> file, const macho::Image& image) {
    return inspect_bind_stream(file,image,StreamKind::eager).sites;
}
LegacyBindStream inspect_legacy_weak_bind_sites(
    std::span<const std::byte> file, const macho::Image& image) {
    return inspect_bind_stream(file,image,StreamKind::weak);
}
std::vector<LegacyBindSite> inspect_legacy_lazy_bind_sites(
    std::span<const std::byte> file, const macho::Image& image) {
    return inspect_bind_stream(file,image,StreamKind::lazy).sites;
}
} // namespace anyios::dyld
