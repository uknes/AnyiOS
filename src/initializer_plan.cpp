#include <anyios/initializer_plan.hpp>
#include <anyios/process_bootstrap.hpp>

#include <algorithm>
#include <utility>
#include <unordered_map>

namespace anyios::loader {

std::vector<InitializerCall> plan_owned_image_initializers(
    std::span<const MappedInitializerImage> images,
    std::string_view executable_path,
    const cpu::GuestMemory& memory) {
    if (images.empty() || images.size() > 256)
        throw macho::FormatError("initializer graph image count out of bounds");
    std::vector<dyld::Module> modules;
    std::unordered_map<std::string, const MappedInitializerImage*> mapped;
    std::size_t executables = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto& input : images) {
        const auto& image = input.module.image;
        if (image.is_fat || image.is_encrypted || (image.file_type != 2 && image.file_type != 6))
            throw macho::FormatError("initializer graph requires unencrypted executable/dylib images");
        if (image.dependencies.size() > 4096 || image.sections.size() > 4096 || input.module.path.size() > 4096)
            throw macho::FormatError("initializer graph metadata limits exceeded");
        if (image.file_type == 2) ++executables;
        for (const auto& dependency : image.dependencies) {
            if (dependency.reexport || dependency.upward)
                throw macho::FormatError("initializer graph reexport/upward dependencies unsupported");
        }
        if (input.guest_text_base % cpu::GuestMemory::ios_page_size != 0 ||
            !mapped.emplace(input.module.path, &input).second)
            throw macho::FormatError("initializer graph duplicate path or unaligned base");
        const auto text = std::find_if(image.segments.begin(), image.segments.end(),
            [](const macho::Segment& segment) { return segment.name == "__TEXT"; });
        if (text == image.segments.end() || image.segments.size() > 128)
            throw macho::FormatError("initializer graph missing text or excess segments");
        for (const auto& segment : image.segments) {
            if (segment.vm_size == 0 || segment.init_protection == 0) continue;
            if (segment.vm_address < text->vm_address || segment.file_size > segment.vm_size ||
                segment.vm_size > 64U * 1024U * 1024U ||
                (segment.init_protection & ~7u) || (segment.init_protection & 6u) == 6u)
                throw macho::FormatError("initializer graph invalid segment layout");
            const auto offset = segment.vm_address - text->vm_address;
            if (offset > UINT64_MAX - input.guest_text_base)
                throw macho::FormatError("initializer graph segment address overflow");
            const auto begin = input.guest_text_base + offset;
            if (segment.vm_size > UINT64_MAX - begin ||
                !memory.allowed(begin, static_cast<std::size_t>(segment.vm_size), cpu::Access::read) ||
                ((segment.init_protection & 4u) &&
                 !memory.allowed(begin, static_cast<std::size_t>(segment.vm_size), cpu::Access::execute)))
                throw macho::FormatError("initializer graph segment unmapped or inaccessible");
            ranges.emplace_back(begin, begin + segment.vm_size);
        }
        modules.push_back(input.module);
    }
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t n = 1; n < ranges.size(); ++n)
        if (ranges[n].first < ranges[n-1].second)
            throw macho::FormatError("initializer graph mapped image segments overlap");
    if (executables != 1)
        throw macho::FormatError("initializer graph requires exactly one executable");
    const auto order = dyld::plan_dependencies(modules, executable_path);
    // An absent weak image may affect launch behavior; this subset does not
    // claim that initializer execution with an incomplete graph is safe.
    if (!order.missing_weak.empty() || order.load_order.size() != images.size())
        throw macho::FormatError("initializer graph missing weak or unreachable image");
    std::vector<InitializerCall> calls;
    for (const auto& path : order.load_order) {
        const auto& input = *mapped.at(path);
        const auto entries = find_owned_module_initializers(
            input.module.image, memory, input.guest_text_base);
        if (entries.size() > 4096 - calls.size())
            throw macho::FormatError("initializer graph call count exceeded");
        for (auto function : entries) calls.push_back({path, function});
    }
    return calls;
}
}
