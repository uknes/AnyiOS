#include <anyios/legacy_rebase.hpp>
#include <anyios/macho.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>
namespace anyios::dyld {
namespace {
constexpr std::uint64_t kMaxSites = 65536;
std::uint64_t add(std::uint64_t a, std::uint64_t b) {
    if (b > UINT64_MAX - a) throw macho::FormatError("legacy rebase address overflow");
    return a + b;
}
std::uint64_t read_uleb(std::span<const std::byte> data, std::size_t& cursor) {
    std::uint64_t result = 0;
    for (unsigned i=0;i<10;++i) {
        if (cursor>=data.size()) throw macho::FormatError("truncated legacy ULEB");
        const auto v = std::to_integer<std::uint8_t>(data[cursor++]);
        if (i==9 && v>1) throw macho::FormatError("legacy ULEB overflow");
        result |= static_cast<std::uint64_t>(v & 0x7fU) << (7*i);
        if ((v & 0x80U)==0) return result;
    }
    throw macho::FormatError("unterminated legacy ULEB");
}
}
std::vector<LegacyRebaseSite> inspect_legacy_rebase_sites(
    std::span<const std::byte> file, const macho::Image& image) {
    if (!image.legacy_dyld || !image.legacy_dyld->rebase) return {};
    const auto range = *image.legacy_dyld->rebase;
    if (range.file_offset > file.size() ||
        range.file_size > file.size()-range.file_offset ||
        range.file_size > 16U*1024U*1024U)
        throw macho::FormatError("legacy rebase stream out of file");
    const auto stream=file.subspan(range.file_offset,range.file_size);
    std::vector<LegacyRebaseSite> sites;
    std::unordered_set<std::uint64_t> seen;
    bool segment_set=false;
    std::uint32_t seg_index=0;
    std::uint64_t seg_offset=0;
    bool pointer_type=false;
    bool terminated=false;
    std::size_t pos=0;
    auto advance=[&](std::uint64_t bytes){seg_offset=add(seg_offset,bytes);};
    auto emit=[&](){
        if (!segment_set || !pointer_type)
            throw macho::FormatError("legacy rebase requires selected segment and pointer type");
        if (seg_index>=image.segments.size())
            throw macho::FormatError("legacy rebase segment index out of range");
        const auto& seg=image.segments[seg_index];
        if ((seg_offset & 7) || seg_offset > seg.file_size ||
            seg.file_size - seg_offset < 8 ||
            seg_offset > seg.vm_size || seg.vm_size - seg_offset < 8 ||
            (seg.init_protection & 1u)==0)
            throw macho::FormatError("legacy rebase pointer outside mapped file-backed segment");
        const auto file_at=add(seg.file_offset,seg_offset);
        if (file_at>file.size() || file.size()-file_at<8)
            throw macho::FormatError("legacy rebase pointer outside input");
        if (sites.size()>=kMaxSites)
            throw macho::FormatError("legacy rebase site safety bound exceeded");
        if (!seen.insert(file_at).second)
            throw macho::FormatError("duplicate legacy rebase site");
        sites.push_back({file_at,add(seg.vm_address,seg_offset)});
    };
    while (pos<stream.size()) {
        const auto raw=std::to_integer<std::uint8_t>(stream[pos++]);
        const auto op=raw & 0xf0U, imm=raw & 0x0fU;
        switch(op) {
            case 0x00:
                if (imm) throw macho::FormatError("invalid legacy rebase DONE");
                terminated=true;
                while (pos<stream.size()) {
                    if (stream[pos++]!=std::byte{0})
                        throw macho::FormatError("trailing legacy rebase instructions after DONE");
                }
                break;
            case 0x10:
                if (imm!=1) throw macho::FormatError("unsupported non-pointer legacy rebase kind");
                pointer_type=true; break;
            case 0x20:
                if (imm>=image.segments.size())
                    throw macho::FormatError("legacy rebase segment index out of range");
                seg_index=imm; seg_offset=read_uleb(stream,pos);
                segment_set=true;break;
            case 0x30: advance(read_uleb(stream,pos));break;
            case 0x40: advance(std::uint64_t(imm)*8);break;
            case 0x50:
                for (unsigned n=0;n<imm;++n){emit();advance(8);}break;
            case 0x60: {
                const auto count=read_uleb(stream,pos);
                if(count>kMaxSites-sites.size())
                    throw macho::FormatError("legacy rebase count safety bound exceeded");
                for(std::uint64_t n=0;n<count;++n){emit();advance(8);}break;
            }
            case 0x70: {
                const auto skip=read_uleb(stream,pos);
                emit();advance(add(skip,8));break;
            }
            case 0x80: {
                const auto count=read_uleb(stream,pos), skip=read_uleb(stream,pos);
                if(count>kMaxSites-sites.size())
                    throw macho::FormatError("legacy rebase count safety bound exceeded");
                const auto stride=add(skip,8);
                for(std::uint64_t n=0;n<count;++n){emit();advance(stride);}break;
            }
            default: throw macho::FormatError("unsupported legacy rebase opcode");
        }
        if (terminated) break;
    }
    if(!terminated) throw macho::FormatError("legacy rebase stream missing DONE");
    return sites; // no mutation or execution in this stage.
}
} // namespace anyios::dyld
