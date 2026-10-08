#include <anyios/process_bootstrap.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace anyios::loader {
namespace {
std::uint64_t checked_add(std::uint64_t x, std::uint64_t y) {
    if (y > UINT64_MAX - x) throw macho::FormatError("process guest pointer overflow");
    return x + y;
}
}

AppleProcessStart prepare_owned_process_stack(
    cpu::GuestMemory& memory, std::uint64_t stack_base,
    std::size_t stack_size,
    std::span<const std::string_view> arguments,
    std::span<const std::string_view> environment,
    std::span<const std::string_view> apple) {
    constexpr auto page = cpu::GuestMemory::ios_page_size;
    if (stack_base % page || stack_size == 0 || stack_size % page ||
        stack_size > 512 * 1024 ||
        arguments.size() < 1 || arguments.size() > 16 ||
        environment.size() > 16 || apple.size() > 16) {
        throw macho::FormatError("invalid owned Apple guest stack configuration");
    }
    const auto end = checked_add(stack_base, stack_size);
    cpu::GuestMemory::MappingJournal journal(memory);
    if (!journal.map(stack_base, stack_size,
                     cpu::bits(cpu::Access::read) | cpu::bits(cpu::Access::write),
                     true)) {
        throw macho::FormatError("owned process stack mapping failed");
    }
    std::uint64_t cursor = end;
    auto strings = [&](std::span<const std::string_view> items) {
        std::vector<std::uint64_t> pointers;
        pointers.reserve(items.size());
        for (auto item : items) {
            if (item.size() > 1024 || item.find('\0') != std::string_view::npos ||
                cursor < stack_base || item.size() + 1 > cursor - stack_base) {
                throw macho::FormatError("invalid owned Apple process string");
            }
            cursor -= item.size() + 1;
            std::vector<std::byte> bytes;
            bytes.reserve(item.size() + 1);
            for (char ch : item) bytes.push_back(std::byte(static_cast<unsigned char>(ch)));
            bytes.push_back(std::byte{0});
            if (!journal.load(cursor, bytes)) {
                throw macho::FormatError("process string write failed");
            }
            pointers.push_back(cursor);
        }
        return pointers;
    };
    const auto arg_ptrs = strings(arguments);
    const auto env_ptrs = strings(environment);
    const auto apple_ptrs = strings(apple);
    cursor &= ~std::uint64_t{15};
    const auto total_words = arguments.size() + environment.size() + apple.size() + 3;
    if (total_words > (cursor - stack_base) / 8) {
        throw macho::FormatError("owned process vectors exceed guest stack");
    }
    cursor = (cursor - total_words * 8) & ~std::uint64_t{15};
    auto put = [&](std::size_t offset, std::uint64_t value) {
        if (!memory.write(cursor + offset * 8, value, 8)) {
            throw macho::FormatError("owned Apple process vector write failed");
        }
    };
    AppleProcessStart result;
    result.sp = cursor;
    result.argc = arg_ptrs.size();
    result.argv = cursor;
    std::size_t i = 0;
    for (auto address : arg_ptrs) put(i++, address);
    put(i++, 0);
    result.envp = cursor + i * 8;
    for (auto address : env_ptrs) put(i++, address);
    put(i++, 0);
    result.apple = cursor + i * 8;
    for (auto address : apple_ptrs) put(i++, address);
    put(i++, 0);
    journal.commit();
    return result;
}

std::vector<std::uint64_t> find_owned_module_initializers(
    const macho::Image& image, const cpu::GuestMemory& memory,
    std::uint64_t guest_image_base) {
    const auto text = std::find_if(image.segments.begin(), image.segments.end(),
        [](const macho::Segment& value) { return value.name == "__TEXT"; });
    if (text == image.segments.end()) {
        throw macho::FormatError("module initializer image has no __TEXT");
    }
    std::vector<std::uint64_t> functions;
    for (const auto& section : image.sections) {
        const bool pointer_array = section.name == "__mod_init_func";
        const bool text_offsets = section.name == "__init_offsets";
        if (!pointer_array && !text_offsets) continue;
        if ((pointer_array &&
             section.segment_name != "__DATA" &&
             section.segment_name != "__DATA_CONST") ||
            (text_offsets && section.segment_name != "__TEXT")) {
            throw macho::FormatError("initializer section in unsupported segment");
        }
        const auto width = pointer_array ? std::uint64_t{8} : std::uint64_t{4};
        if (section.size == 0 || section.size > 64 * width ||
            section.size % width != 0 ||
            section.address < text->vm_address) {
            throw macho::FormatError("invalid owned initializer section");
        }
        const auto offset = section.address - text->vm_address;
        const auto address = checked_add(guest_image_base, offset);
        for (std::uint64_t i = 0; i < section.size; i += width) {
            const auto raw = memory.read(checked_add(address, i),
                                         static_cast<unsigned>(width));
            if (!raw) throw macho::FormatError("initializer descriptor is unreadable");
            const auto target = text_offsets
                ? checked_add(guest_image_base, *raw) : *raw;
            if (!memory.fetch(target)) {
                throw macho::FormatError("initializer target unmapped or not executable");
            }
            functions.push_back(target);
        }
    }
    return functions;
}
}
