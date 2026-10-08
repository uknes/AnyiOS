#include <anyios/linked_image.hpp>
#include <anyios/fixup_plan.hpp>
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
    // Linked code can use modern chained fixups only. Legacy bind opcodes and
    // arm64e pointer authentication are intentionally not executed here.
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
        if (type == 0x80000022 || type == 0x22 || type == 0x5) {
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

    auto patches = dyld::plan_chained_fixups(file, image, resolved_imports);
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
    for (const auto& segment : image.segments) {
        if (segment.name == "__PAGEZERO" && segment.file_size == 0 &&
            segment.init_protection == 0) continue;
        std::uint64_t mapping_size = segment.vm_size;
        if (options.require_ios_pages) {
            if (segment.vm_address % cpu::GuestMemory::ios_page_size != 0) {
                throw macho::FormatError("linked iOS segment " + segment.name +
                                         " violates 16 KiB guest page alignment");
            }
            if (segment.vm_size % cpu::GuestMemory::ios_page_size != 0) {
                const auto safe_tail = segment.name == "__LINKEDIT" &&
                                       &segment == &image.segments.back() &&
                                       segment.init_protection == 1 &&
                                       segment.file_size <= segment.vm_size &&
                                       segment.vm_size > 0;
                if (!safe_tail ||
                    segment.vm_size > UINT64_MAX -
                        (cpu::GuestMemory::ios_page_size - 1)) {
                    throw macho::FormatError("linked iOS segment " + segment.name +
                                             " violates 16 KiB guest page alignment");
                }
                mapping_size = (segment.vm_size +
                    cpu::GuestMemory::ios_page_size - 1) &
                    ~std::uint64_t(cpu::GuestMemory::ios_page_size - 1);
            }
        }
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
}
