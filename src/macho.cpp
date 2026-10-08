#include <anyios/macho.hpp>
#include "internal.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string_view>

namespace anyios::macho {
namespace {

constexpr std::uint32_t magic_64 = 0xfeedfacf;
constexpr std::uint32_t cpu_arm64 = 0x0100000c;
constexpr std::uint32_t lc_segment_64 = 0x19;
constexpr std::uint32_t lc_load_dylib = 0x0c;
constexpr std::uint32_t lc_load_weak_dylib = 0x80000018;
constexpr std::uint32_t lc_reexport_dylib = 0x8000001f;
constexpr std::uint32_t lc_load_upward_dylib = 0x80000023;
constexpr std::uint32_t lc_rpath = 0x8000001c;
constexpr std::uint32_t lc_main = 0x80000028;
constexpr std::uint32_t lc_build_version = 0x32;
constexpr std::uint32_t lc_version_min_iphoneos = 0x25;
constexpr std::uint32_t lc_encryption_info_64 = 0x2c;
constexpr std::uint32_t lc_exports_trie = 0x80000033;
constexpr std::uint32_t lc_chained_fixups = 0x80000034;
constexpr std::uint32_t lc_symtab = 0x2;
constexpr std::uint32_t lc_dysymtab = 0xb;

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    void require(std::size_t offset, std::size_t size, std::string_view item) const {
        if (offset > bytes_.size() || size > bytes_.size() - offset) {
            throw FormatError(std::string(item) + " out of bounds");
        }
    }

    std::uint32_t u32(std::size_t offset, std::string_view item) const {
        require(offset, 4, item);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            value |= std::uint32_t(std::to_integer<unsigned char>(bytes_[offset + i])) << (i * 8);
        }
        return value;
    }

    std::uint64_t u64(std::size_t offset, std::string_view item) const {
        return std::uint64_t(u32(offset, item)) | (std::uint64_t(u32(offset + 4, item)) << 32);
    }

    std::string fixed_name(std::size_t offset, std::size_t size) const {
        require(offset, size, "segment name");
        std::size_t end = 0;
        while (end < size && bytes_[offset + end] != std::byte{0}) ++end;
        return {reinterpret_cast<const char*>(bytes_.data() + offset), end};
    }

    std::string command_string(std::size_t command, std::size_t size, std::uint32_t offset,
                               std::size_t minimum_offset, std::string_view item) const {
        if (offset < minimum_offset || offset >= size) {
            throw FormatError(std::string(item) + " offset is outside its command");
        }
        std::size_t i = offset;
        while (i < size && bytes_[command + i] != std::byte{0}) ++i;
        if (i == size) throw FormatError(std::string(item) + " is not NUL-terminated");
        return {reinterpret_cast<const char*>(bytes_.data() + command + offset), i - offset};
    }

private:
    std::span<const std::byte> bytes_;
};

bool is_library_command(std::uint32_t command) {
    return command == lc_load_dylib || command == lc_load_weak_dylib ||
           command == lc_reexport_dylib || command == lc_load_upward_dylib;
}

}

Image inspect_thin(std::span<const std::byte> bytes) {
    Reader reader(bytes);
    reader.require(0, 4, "Mach-O magic");
    const auto magic = reader.u32(0, "Mach-O magic");
    if (magic != magic_64) {
        throw FormatError("unsupported format: expected a thin little-endian 64-bit Mach-O");
    }
    reader.require(0, 32, "Mach-O header");
    if (reader.u32(4, "CPU type") != cpu_arm64) {
        throw FormatError("unsupported CPU: only ARM64 Mach-O is supported");
    }

    Image image;
    image.cpu_subtype = reader.u32(8, "CPU subtype");
    image.file_type = reader.u32(12, "file type");
    image.command_count = reader.u32(16, "load command count");
    if (image.command_count > 16384) throw FormatError("load command count exceeds safety limit");
    const auto commands_size = reader.u32(20, "load command region size");
    reader.require(32, commands_size, "load command region");
    if (image.command_count > commands_size / 8) {
        throw FormatError("load command count exceeds region capacity");
    }

    struct SymbolTable { std::uint32_t symbols_offset, count, strings_offset, strings_size; };
    std::optional<SymbolTable> symtab;
    std::optional<std::pair<std::uint32_t, std::uint32_t>> indirect;
    std::optional<LinkeditRange> chained;
    std::optional<LinkeditRange> exports;
    std::size_t cursor = 32;
    const std::size_t commands_end = cursor + commands_size;
    for (std::uint32_t index = 0; index < image.command_count; ++index) {
        if (commands_end - cursor < 8) throw FormatError("truncated load command header");
        const auto command = reader.u32(cursor, "load command type");
        const auto size = reader.u32(cursor + 4, "load command size");
        if (size < 8 || size % 8 != 0 || size > commands_end - cursor) {
            throw FormatError("invalid load command size or alignment");
        }
        if (command == lc_segment_64) {
            if (size < 72) throw FormatError("truncated LC_SEGMENT_64");
            Segment segment{reader.fixed_name(cursor + 8, 16),
                            reader.u64(cursor + 24, "segment vmaddr"),
                            reader.u64(cursor + 32, "segment vmsize"),
                            reader.u64(cursor + 40, "segment fileoff"),
                            reader.u64(cursor + 48, "segment filesize"),
                            reader.u32(cursor + 64, "segment section count")};
            if (segment.sections > (size - 72) / 80) {
                throw FormatError("LC_SEGMENT_64 sections exceed command size");
            }
            if (segment.file_offset > bytes.size() ||
                segment.file_size > bytes.size() - segment.file_offset) {
                throw FormatError("segment file range out of bounds");
            }
            if (segment.vm_size < segment.file_size ||
                segment.vm_address > std::numeric_limits<std::uint64_t>::max() - segment.vm_size) {
                throw FormatError("invalid segment virtual memory range");
            }
            for (std::uint32_t j = 0; j < segment.sections; ++j) {
                const auto at = cursor + 72 + std::size_t(j) * 80;
                const auto flags = reader.u32(at + 64, "section flags");
                const auto type = flags & 0xff;
                const bool zero_fill = type == 1 || type == 0xc || type == 0x12;
                Section section{reader.fixed_name(at, 16), reader.fixed_name(at + 16, 16),
                                reader.u64(at + 32, "section address"),
                                reader.u64(at + 40, "section size"),
                                reader.u32(at + 48, "section offset"),
                                reader.u32(at + 60, "section relocation count"), zero_fill};
                if (!zero_fill) reader.require(section.file_offset, static_cast<std::size_t>(section.size), "section contents");
                reader.require(reader.u32(at + 56, "section relocation offset"),
                               std::size_t(section.relocation_count) * 8, "section relocations");
                image.sections.push_back(std::move(section));
            }
            image.segments.push_back(std::move(segment));
        } else if (command == lc_symtab) {
            if (size < 24) throw FormatError("truncated LC_SYMTAB");
            if (symtab) throw FormatError("duplicate LC_SYMTAB");
            symtab = SymbolTable{reader.u32(cursor + 8, "symbols offset"),
                                 reader.u32(cursor + 12, "symbol count"),
                                 reader.u32(cursor + 16, "strings offset"),
                                 reader.u32(cursor + 20, "strings size")};
        } else if (command == lc_dysymtab) {
            if (size < 80) throw FormatError("truncated LC_DYSYMTAB");
            if (indirect) throw FormatError("duplicate LC_DYSYMTAB");
            indirect = std::pair<std::uint32_t, std::uint32_t>{
                reader.u32(cursor + 56, "indirect symbol offset"),
                reader.u32(cursor + 60, "indirect symbol count")};
        } else if (is_library_command(command)) {
            if (size < 24) throw FormatError("truncated dylib command");
            image.libraries.push_back(reader.command_string(cursor, size,
                reader.u32(cursor + 8, "dylib name offset"), 24, "dylib name"));
        } else if (command == lc_rpath) {
            if (size < 12) throw FormatError("truncated LC_RPATH");
            image.rpaths.push_back(reader.command_string(cursor, size,
                reader.u32(cursor + 8, "rpath offset"), 12, "rpath"));
        } else if (command == lc_main) {
            if (size < 24) throw FormatError("truncated LC_MAIN");
            if (image.has_entry) throw FormatError("duplicate LC_MAIN");
            image.has_entry = true;
            image.entry_offset = reader.u64(cursor + 8, "entry offset");
        } else if (command == lc_build_version) {
            if (size < 24) throw FormatError("truncated LC_BUILD_VERSION");
            if (reader.u32(cursor + 20, "build tool count") > (size - 24) / 8) {
                throw FormatError("LC_BUILD_VERSION tools exceed command size");
            }
            image.versions.push_back({reader.u32(cursor + 8, "platform"),
                                      reader.u32(cursor + 12, "minimum OS"),
                                      reader.u32(cursor + 16, "SDK")});
        } else if (command == lc_version_min_iphoneos) {
            if (size < 16) throw FormatError("truncated LC_VERSION_MIN_IPHONEOS");
            image.versions.push_back({2, reader.u32(cursor + 8, "minimum OS"),
                                      reader.u32(cursor + 12, "SDK")});
        } else if (command == lc_exports_trie || command == lc_chained_fixups) {
            if (size < 16) throw FormatError("truncated linkedit data command");
            LinkeditRange range{reader.u32(cursor + 8, "linkedit data offset"),
                                reader.u32(cursor + 12, "linkedit data size")};
            reader.require(range.file_offset, range.file_size, "linkedit data");
            auto& slot = command == lc_chained_fixups ? chained : exports;
            if (slot.has_value()) throw FormatError("duplicate linkedit command");
            slot = range;
        } else if (command == lc_encryption_info_64) {
            if (size < 24) throw FormatError("truncated LC_ENCRYPTION_INFO_64");
            const auto offset = reader.u32(cursor + 8, "encrypted offset");
            const auto length = reader.u32(cursor + 12, "encrypted size");
            reader.require(offset, length, "encrypted file range");
            if (reader.u32(cursor + 16, "cryptid") != 0) image.is_encrypted = true;
        }
        cursor += size;
    }
    if (cursor != commands_end) throw FormatError("load command count does not consume declared region");
    if (indirect && !symtab) throw FormatError("LC_DYSYMTAB requires LC_SYMTAB");
    if (indirect) {
        if (indirect->second > 1000000) throw FormatError("indirect symbol count exceeds safety limit");
        reader.require(indirect->first, std::size_t(indirect->second) * 4, "indirect symbol table");
        image.indirect_symbol_count = indirect->second;
    }
    if (symtab) {
        image.symbols = parse_symbols(bytes, symtab->symbols_offset, symtab->count,
                                      symtab->strings_offset, symtab->strings_size);
    }
    if (chained) {
        image.has_chained_fixups = true;
        image.chained_imports = parse_chained_imports(bytes, chained->file_offset, chained->file_size);
    }
    if (exports) {
        image.has_export_trie = true;
        image.exported_symbols = parse_exports(bytes, exports->file_offset, exports->file_size);
    }
    return image;
}

std::string version_string(std::uint32_t encoded) {
    return std::to_string(encoded >> 16) + "." +
           std::to_string((encoded >> 8) & 0xff) + "." +
           std::to_string(encoded & 0xff);
}

}
