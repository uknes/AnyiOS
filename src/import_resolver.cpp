#include <anyios/import_resolver.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace anyios::dyld {
namespace {
std::uint32_t read32(std::span<const std::byte> bytes, std::uint64_t at) {
    if (at > bytes.size() || bytes.size() - at < 4) {
        throw macho::FormatError("chained import descriptor out of bounds");
    }
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= std::uint32_t(std::to_integer<std::uint8_t>(
            bytes[static_cast<std::size_t>(at) + i])) << (8 * i);
    }
    return value;
}
std::uint64_t slide(std::uint64_t base, std::uint64_t offset) {
    if (offset > std::numeric_limits<std::uint64_t>::max() - base) {
        throw macho::FormatError("resolved symbol address overflow");
    }
    return base + offset;
}
bool exported(const macho::Symbol& symbol) {
    constexpr std::uint8_t n_stab = 0xe0;
    constexpr std::uint8_t n_private = 0x10;
    constexpr std::uint8_t n_ext = 0x01;
    constexpr std::uint8_t n_type = 0x0e;
    constexpr std::uint8_t n_sect = 0x0e;
    constexpr std::uint8_t n_abs = 0x02;
    if (symbol.name.empty() || (symbol.type & (n_stab | n_private)) != 0 ||
        (symbol.type & n_ext) == 0) return false;
    const auto kind = symbol.type & n_type;
    return (kind == n_sect && symbol.section_index != 0) || kind == n_abs;
}
}

std::vector<std::uint64_t> resolve_chained_import_targets(
    std::span<const std::byte> importing_file,
    const macho::Image& importer,
    std::span<const LoadedDylib> loaded_libraries) {
    if (!importer.chained_fixups_range) {
        if (!importer.chained_imports.empty()) {
            throw macho::FormatError("chained import names without fixup header");
        }
        return {};
    }
    const auto& range = *importer.chained_fixups_range;
    if (range.file_offset > importing_file.size() ||
        range.file_size > importing_file.size() - range.file_offset ||
        range.file_size < 28) {
        throw macho::FormatError("chained import payload out of bounds");
    }
    const auto payload = importing_file.subspan(range.file_offset, range.file_size);
    const auto count = read32(payload, 16);
    const auto format = read32(payload, 20);
    if (format < 1 || format > 3) {
        throw macho::FormatError("unsupported chained import addend format");
    }
    if (count != importer.chained_imports.size() || count > 100000) {
        throw macho::FormatError("chained import names/count mismatch");
    }
    const auto table = read32(payload, 8);
    const std::uint64_t entry_size = format == 1 ? 4 : format == 2 ? 8 : 16;
    if (table > payload.size() || std::uint64_t(count) * entry_size > payload.size() - table) {
        throw macho::FormatError("chained import table out of bounds");
    }
    if (loaded_libraries.size() > 4096) throw macho::FormatError("too many loaded dylibs");
    std::unordered_map<std::string, const LoadedDylib*> installed;
    for (const auto& lib : loaded_libraries) {
        if (!lib.image || lib.image->file_type != 6 || lib.image->install_name.empty() ||
            !installed.emplace(lib.image->install_name, &lib).second) {
            throw macho::FormatError("invalid or duplicate loaded dylib install name");
        }
    }

    std::vector<std::uint64_t> resolved;
    resolved.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto offset = std::uint64_t(table) + std::uint64_t(index) * entry_size;
        const auto low = read32(payload, offset);
        const auto upper = format == 3 ? read32(payload, offset + 4) : 0U;
        const auto ordinal = format == 3 ? (low & 0xffff) : (low & 0xff);
        const auto weak = (format == 3 ? (low & 0x10000) : (low & 0x100)) != 0;
        if (format == 3 && (low & 0xfffe0000U) != 0) {
            throw macho::FormatError("chained import64 reserved bits set");
        }
        static_cast<void>(upper);
        if (ordinal == 0 || ordinal > importer.dependencies.size()) {
            throw macho::FormatError("unsupported chained import library ordinal");
        }
        const auto& name = importer.dependencies[ordinal - 1].install_name;
        const auto found = installed.find(name);
        if (found == installed.end()) {
            throw macho::FormatError("chained import dependency not loaded: " + name);
        }
        const auto& library = *found->second;
        bool matched = false;
        std::uint64_t target = 0;
        for (const auto& symbol : library.image->symbols) {
            if (symbol.name != importer.chained_imports[index] || !exported(symbol)) continue;
            if (matched) throw macho::FormatError("ambiguous chained exported symbol");
            matched = true;
            const bool absolute = (symbol.type & 0x0e) == 0x02;
            target = absolute ? symbol.value : slide(symbol.value, library.slide);
        }
        if (!matched) {
            for (const auto& entry : library.image->exports) {
                if (entry.name != importer.chained_imports[index]) continue;
                if (matched) throw macho::FormatError("ambiguous chained exported symbol");
                if ((entry.flags & 0x18) != 0 || (entry.flags & 3) == 1) {
                    throw macho::FormatError("unsupported reexport, resolver or TLS symbol");
                }
                if ((entry.flags & 3) > 2) {
                    throw macho::FormatError("unsupported export symbol kind");
                }
                const bool absolute = (entry.flags & 3) == 2;
                if (absolute) {
                    target = entry.address;
                } else {
                    const auto text = std::find_if(library.image->segments.begin(),
                        library.image->segments.end(),
                        [](const macho::Segment& seg) { return seg.name == "__TEXT"; });
                    if (text == library.image->segments.end()) {
                        throw macho::FormatError("export trie target missing __TEXT base");
                    }
                    target = slide(slide(text->vm_address, entry.address), library.slide);
                }
                matched = true;
            }
        }
        if (!matched || target == 0) {
            if (weak) {
                throw macho::FormatError("unresolved weak chained bind unsupported");
            }
            throw macho::FormatError("unresolved chained exported symbol: " +
                                     importer.chained_imports[index]);
        }
        resolved.push_back(target);
    }
    return resolved;
}
}
