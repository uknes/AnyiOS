#include <anyios/legacy_fixups.hpp>
#include <anyios/legacy_bind.hpp>
#include <anyios/legacy_rebase.hpp>
#include <unordered_set>
namespace anyios::dyld {
namespace {
std::uint64_t read_pointer(std::span<const std::byte> file, std::uint64_t at) {
    if(at>file.size() || file.size()-at<8)
        throw macho::FormatError("legacy rebase pointer outside file");
    std::uint64_t result=0;
    for(unsigned i=0;i<8;++i)
        result |= std::uint64_t(std::to_integer<std::uint8_t>(file[static_cast<std::size_t>(at)+i])) << (8*i);
    return result;
}
std::uint64_t addend(std::uint64_t target,std::int64_t delta) {
    if(delta>=0) {
        const auto n=static_cast<std::uint64_t>(delta);
        if(n>UINT64_MAX-target)throw macho::FormatError("legacy bind addend overflow");
        return target+n;
    }
    const auto n=static_cast<std::uint64_t>(-(delta+1))+1;
    if(n>target)throw macho::FormatError("legacy bind addend underflow");
    return target-n;
}
}
std::vector<FixupPatch> plan_legacy_fixups(std::span<const std::byte> file,
    const macho::Image& image,std::span<const std::uint64_t> targets) {
    if(!image.legacy_dyld || image.has_chained_fixups || image.is_fat || image.is_encrypted)
        throw macho::FormatError("legacy fixups require thin unencrypted legacy-only image");
    if(image.legacy_dyld->weak_bind && image.legacy_dyld->weak_bind->file_size)
        throw macho::FormatError("legacy weak coalescing not implemented");
    auto eager=inspect_legacy_eager_bind_sites(file,image);
    const auto lazy=inspect_legacy_lazy_bind_sites(file,image);
    eager.insert(eager.end(),lazy.begin(),lazy.end());
    if(eager.size()>65536 || targets.size()!=eager.size())
        throw macho::FormatError("legacy bind target count mismatch or cap exceeded");
    std::vector<FixupPatch> patches;
    std::unordered_set<std::uint64_t> bound;
    for(std::size_t i=0;i<eager.size();++i) {
        const auto& site=eager[i];
        if(!bound.insert(site.file_offset).second)
            throw macho::FormatError("duplicate eager/lazy legacy bind target");
        if(targets[i]==0)
            throw macho::FormatError("unresolved legacy bind target");
        const auto value=addend(targets[i],site.addend);
        if(!value)throw macho::FormatError("legacy addend produced null target");
        patches.push_back({site.file_offset,value,true});
    }
    // An ordinary lazy pointer can be both rebased to its stub helper and
    // prebound to its resolved function. Binding supersedes that rebase.
    for(const auto& site:inspect_legacy_rebase_sites(file,image)) {
        if(bound.contains(site.file_offset))continue;
        if(patches.size()>=65536)throw macho::FormatError("legacy fixup cap exceeded");
        patches.push_back({site.file_offset,read_pointer(file,site.file_offset),false});
    }
    return patches;
}
}
