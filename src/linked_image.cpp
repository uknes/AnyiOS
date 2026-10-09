#include <anyios/linked_image.hpp>
#include <anyios/fixup_plan.hpp>
#include <anyios/legacy_fixups.hpp>
#include <anyios/macho.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace anyios::loader {
namespace {
std::uint64_t sum(std::uint64_t a, std::uint64_t b, const char* error) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        throw macho::FormatError(error);
    }
    return a + b;
}
bool inside(std::uint64_t at, std::uint64_t bytes, std::uint64_t begin,
            std::uint64_t length) {
    return at >= begin && at - begin <= length && bytes <= length - (at - begin);
}
}

std::uint64_t ios_segment_mapping_size(const macho::Image& image, std::size_t index) {
    if (index >= image.segments.size()) throw macho::FormatError("linked segment index out of bounds");
    const auto& segment = image.segments[index];
    constexpr auto page = cpu::GuestMemory::ios_page_size;
    if (segment.file_size > segment.vm_size)
        throw macho::FormatError("linked segment file extent exceeds virtual extent");
    if (segment.vm_address % page != 0 || segment.vm_size == 0)
        throw macho::FormatError("linked iOS segment " + segment.name + " violates 16 KiB guest page alignment");
    auto size = segment.vm_size;
    if (size % page != 0) {
        const bool safe_tail = segment.name == "__LINKEDIT" && index + 1 == image.segments.size() &&
                               segment.init_protection == 1 && segment.file_size <= size;
        if (!safe_tail || size > UINT64_MAX - (page - 1))
            throw macho::FormatError("linked iOS segment " + segment.name + " violates 16 KiB guest page alignment");
        size = (size + page - 1) & ~std::uint64_t(page - 1);
    }
    if (size > 64U * 1024U * 1024U) throw macho::FormatError("linked iOS segment mapping exceeds safety limit");
    if (size > UINT64_MAX - segment.vm_address) throw macho::FormatError("linked iOS rounded virtual range overflow");
    return size;
}

LinkedImage stage_linked_image(
    std::span<const std::byte> file,
    cpu::GuestMemory& memory,
    std::uint64_t guest_base,
    std::span<const std::uint64_t> resolved_imports,
    LinkedImageOptions options) {
    const auto image = macho::inspect(file);
    if (image.is_fat || image.is_encrypted || (image.file_type != 2 && image.file_type != 6)) {
        throw macho::FormatError("linked mapper requires thin, unencrypted ARM64 executable/dylib");
    }
    const auto page = options.require_ios_pages ?
        cpu::GuestMemory::ios_page_size : cpu::GuestMemory::page_size;
    if (guest_base % page != 0 || image.segments.size() > 128) {
        throw macho::FormatError("invalid guest linked-image configuration");
    }
    if (image.file_type == 2 && !image.has_entry) {
        throw macho::FormatError("linked executable entry point missing");
    }
    const auto text = std::find_if(image.segments.begin(), image.segments.end(),
        [](const macho::Segment& segment) { return segment.name == "__TEXT"; });
    if (text == image.segments.end() || text->file_size < 32 ||
        (text->init_protection & 5u) != 5u || text->file_offset != 0) {
        throw macho::FormatError("linked image has invalid __TEXT base");
    }
    const auto original_base = text->vm_address;
    // Legacy linking requires an explicit caller opt-in and resolved targets.
    // Thread-state entry and arm64e remain unsupported.
    if (file.size() < 32) throw macho::FormatError("linked header truncated");
    auto command = std::size_t{32};
    for (std::uint32_t i = 0; i < image.command_count; ++i) {
        if (command > file.size() || file.size() - command < 8) {
            throw macho::FormatError("linked command header invalid");
        }
        std::uint32_t type = 0, bytes = 0;
        for (unsigned b = 0; b < 4; ++b) {
            type |= std::uint32_t(std::to_integer<std::uint8_t>(file[command + b])) << (8 * b);
            bytes |= std::uint32_t(std::to_integer<std::uint8_t>(file[command + b + 4])) << (8 * b);
        }
        if (((type == 0x80000022 || type == 0x22) && !options.allow_legacy_fixups) || type == 0x5) {
            throw macho::FormatError("unsupported linked image legacy dyld/thread state");
        }
        if (bytes < 8 || bytes > file.size() - command) {
            throw macho::FormatError("linked command range invalid");
        }
        command += bytes;
    }
    if (command > text->file_size) {
        throw macho::FormatError("linked __TEXT does not cover its load commands");
    }

    auto patches = image.legacy_dyld && options.allow_legacy_fixups ?
        dyld::plan_legacy_fixups(file,image,resolved_imports) :
        dyld::plan_chained_fixups(file, image, resolved_imports);
    auto staged_file = std::vector<std::byte>(file.begin(), file.end());
    for (auto& patch : patches) {
        bool stored_in_segment = false;
        for (const auto& segment : image.segments) {
            if (inside(patch.file_offset, 8, segment.file_offset, segment.file_size)) {
                stored_in_segment = true;
                break;
            }
        }
        if (!stored_in_segment) {
            throw macho::FormatError("linked fixup not in a mapped file segment");
        }
        if (patch.binding) continue;
        bool valid_target = false;
        for (const auto& segment : image.segments) {
            if (segment.vm_size && segment.init_protection != 0 &&
                inside(patch.value, 1, segment.vm_address, segment.vm_size)) {
                valid_target = true;
                break;
            }
        }
        if (!valid_target || patch.value < original_base) {
            throw macho::FormatError("linked rebase target outside image");
        }
        patch.value = sum(guest_base, patch.value - original_base, "linked guest rebase overflow");
    }
    dyld::apply_chained_patches(staged_file, patches);

    cpu::GuestMemory::MappingJournal local(memory);
    auto& journal = options.transaction ? *options.transaction : local;
    std::size_t mapped = 0;
    for (std::size_t index = 0; index < image.segments.size(); ++index) {
        const auto& segment = image.segments[index];
        if (segment.name == "__PAGEZERO" && segment.file_size == 0 &&
            segment.init_protection == 0) continue;
        const auto mapping_size = options.require_ios_pages ?
            ios_segment_mapping_size(image, index) : segment.vm_size;
        if (segment.vm_size == 0 || mapping_size % page != 0 ||
            segment.vm_address < original_base ||
            (segment.vm_address - original_base) % page != 0 ||
            mapping_size > 64 * 1024 * 1024) {
            throw macho::FormatError("invalid linked segment layout: " + segment.name);
        }
        const auto perms = segment.init_protection;
        if (perms == 0 || (perms & ~7u) != 0 ||
            (perms & ~segment.max_protection) != 0 ||
            ((perms & 6u) == 6u)) {
            throw macho::FormatError("unsupported linked segment protections");
        }
        const auto address = sum(guest_base, segment.vm_address - original_base,
                                 "linked segment address overflow");

        if (!journal.map(address, static_cast<std::size_t>(mapping_size),
                         perms, options.require_ios_pages)) {
            throw macho::FormatError("linked guest mapping failed");
        }
        if (segment.file_size &&
            !journal.load(address, std::span<const std::byte>(
                staged_file.data() + static_cast<std::size_t>(segment.file_offset),
                static_cast<std::size_t>(segment.file_size)))) {
            throw macho::FormatError("linked guest segment initialization failed");
        }
        ++mapped;
    }

    std::uint64_t entry = 0;
    if (image.has_entry) {
        if (image.entry_offset > text->file_size ||
            text->file_size - image.entry_offset < 4 ||
            (image.entry_offset % 4) != 0) {
            throw macho::FormatError("linked entry point outside __TEXT");
        }
        entry = sum(guest_base, image.entry_offset, "linked entry point overflow");
        if (!memory.fetch(entry)) {
            throw macho::FormatError("linked entry point not executable");
        }
    }
    if (!options.transaction) local.commit();
    return {guest_base, entry, mapped, patches.size()};
}

std::vector<LinkedImage> stage_linked_images(std::span<const LinkedImageInput> inputs,
                                            cpu::GuestMemory& memory,
                                            bool require_ios_pages) {
    if (inputs.empty() || inputs.size() > 256) {
        throw macho::FormatError("invalid linked image set size");
    }
    std::vector<macho::Image> images;
    std::size_t bytes = 0, executables = 0;
    for (const auto& input : inputs) {
        constexpr std::size_t limit = 256U * 1024U * 1024U;
        if (input.file.size() > limit - bytes) {
            throw macho::FormatError("linked image set exceeds file byte limit");
        }
        bytes += input.file.size();
        images.push_back(macho::inspect(input.file));
        if (images.back().file_type == 2) ++executables;
    }
    if (executables != 1) throw macho::FormatError("linked image set requires one executable");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (std::size_t n = 0; n < images.size(); ++n) {
        const auto& image = images[n];
        if (image.segments.size() > 128) throw macho::FormatError("linked image set segment limit exceeded");
        const auto text = std::find_if(image.segments.begin(), image.segments.end(),
            [](const macho::Segment& segment) { return segment.name == "__TEXT"; });
        if (text == image.segments.end()) throw macho::FormatError("linked image set missing __TEXT");
        for (const auto& segment : image.segments) {
            if (!(segment.init_protection & 1u) || segment.vm_size == 0) continue;
            if (segment.vm_address < text->vm_address) throw macho::FormatError("linked image set segment before __TEXT");
            const auto begin = sum(inputs[n].guest_base, segment.vm_address - text->vm_address,
                                   "linked image set target address overflow");
            ranges.emplace_back(begin, sum(begin, segment.vm_size, "linked image set segment overflow"));
        }
    }
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t n = 1; n < ranges.size(); ++n) {
        if (ranges[n].first < ranges[n-1].second) throw macho::FormatError("linked image set readable segments overlap");
    }
    auto contains_target = [&](std::uint64_t target) {
        auto next = std::upper_bound(ranges.begin(), ranges.end(), target,
            [](std::uint64_t value, const auto& range) { return value < range.first; });
        return next != ranges.begin() && target < std::prev(next)->second;
    };
    // Check final values, including signed addends, before publishing any pages.
    // A target in rounded page padding or a preexisting trap page is not owned.
    std::vector<std::uint64_t> targets;
    for (std::size_t n = 0; n < inputs.size(); ++n) {
        const auto& input = inputs[n];
        const auto patches = images[n].legacy_dyld && input.allow_legacy_fixups ?
            dyld::plan_legacy_fixups(input.file, images[n], input.resolved_imports) :
            dyld::plan_chained_fixups(input.file, images[n], input.resolved_imports);
        for (const auto& patch : patches) {
            if (!patch.binding) continue;
            if (targets.size() >= 65536 || !contains_target(patch.value)) {
                throw macho::FormatError("linked binding target outside image set or limit exceeded");
            }
            targets.push_back(patch.value);
        }
    }
    cpu::GuestMemory::MappingJournal journal(memory);
    std::vector<LinkedImage> result;
    result.reserve(inputs.size());
    for (const auto& input : inputs) {
        result.push_back(stage_linked_image(input.file, memory, input.guest_base,
            input.resolved_imports, {require_ios_pages, &journal, input.allow_legacy_fixups}));
    }
    for (const auto target : targets) {
        if (!memory.read(target, 1)) throw macho::FormatError("linked image set target not readable");
    }
    journal.commit();
    return result;
}

}
