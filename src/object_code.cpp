#include <anyios/object_code.hpp>
#include <anyios/macho.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace anyios::loader {
ObjectFunction extract_object_function(std::span<const std::byte> input,
                                       std::string_view name) {
    const auto image = macho::inspect(input);
    if (image.is_encrypted) throw macho::FormatError("encrypted guest code is not supported");
    if (image.file_type != 1) throw macho::FormatError("only owned MH_OBJECT code is supported");

    if (image.is_fat) {
        if (image.slice_offset > input.size() ||
            image.slice_size > input.size() - image.slice_offset) {
            throw macho::FormatError("selected object slice is out of bounds");
        }
        input = input.subspan(static_cast<std::size_t>(image.slice_offset),
                              static_cast<std::size_t>(image.slice_size));
    }

    const macho::Symbol* selected = nullptr;
    for (const auto& symbol : image.symbols) {
        if (symbol.name == name) {
            if (selected) throw macho::FormatError("duplicate requested symbol");
            selected = &symbol;
        }
    }
    if (!selected) throw macho::FormatError("requested symbol not found");
    if ((selected->type & 0x0e) != 0x0e ||
        selected->section_index == 0 ||
        selected->section_index > image.sections.size()) {
        throw macho::FormatError("requested symbol is not defined in a section");
    }

    const auto& section = image.sections[selected->section_index - 1];
    if (section.segment_name != "__TEXT" || section.name != "__text" || section.zero_fill) {
        throw macho::FormatError("requested symbol is not in a supported text section");
    }
    if (section.relocation_count != 0) {
        throw macho::FormatError("relocated object code cannot be executed directly");
    }
    if (selected->value < section.address ||
        selected->value - section.address >= section.size) {
        throw macho::FormatError("requested symbol value is outside its section");
    }

    const std::uint64_t start = selected->value - section.address;
    std::uint64_t end = section.size;
    for (const auto& symbol : image.symbols) {
        if (&symbol != selected && symbol.section_index == selected->section_index &&
            (symbol.type & 0x0e) == 0x0e && symbol.value > selected->value &&
            symbol.value >= section.address) {
            end = std::min(end, symbol.value - section.address);
        }
    }
    if (end <= start || end - start > 64 * 1024 || ((end - start) & 3) != 0) {
        throw macho::FormatError("object function has invalid ARM64 length");
    }
    if (section.file_offset > input.size() ||
        section.size > input.size() - section.file_offset ||
        start > section.size || end > section.size) {
        throw macho::FormatError("object function code range out of bounds");
    }

    const auto first = static_cast<std::size_t>(section.file_offset + start);
    const auto last = static_cast<std::size_t>(section.file_offset + end);
    return {std::vector<std::byte>(input.begin() + static_cast<std::ptrdiff_t>(first),
                                   input.begin() + static_cast<std::ptrdiff_t>(last))};
}
}
