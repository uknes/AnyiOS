#include "internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace anyios::macho {
namespace {
class View {
public:
    explicit View(std::span<const std::byte> data) : data_(data) {}

    void require(std::size_t off, std::size_t length, std::string_view context) const {
        if (off > data_.size() || length > data_.size() - off) {
            throw FormatError(std::string(context) + " out of bounds");
        }
    }
    std::uint8_t u8(std::size_t off, std::string_view context) const {
        require(off, 1, context);
        return std::to_integer<std::uint8_t>(data_[off]);
    }
    std::uint32_t u32(std::size_t off, std::string_view context) const {
        require(off, 4, context);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(u8(off + i, context)) << (8 * i);
        return value;
    }
    std::uint64_t u64(std::size_t off, std::string_view context) const {
        return std::uint64_t(u32(off, context)) | (std::uint64_t(u32(off + 4, context)) << 32);
    }
    std::string string(std::size_t off, std::string_view context) const {
        require(off, 1, context);
        auto end = off;
        while (end < data_.size() && data_[end] != std::byte{0} && end - off < 16384) ++end;
        if (end - off == 16384) throw FormatError(std::string(context) + " exceeds symbol length limit");
        if (end == data_.size()) throw FormatError(std::string(context) + " is not NUL-terminated");
        return {reinterpret_cast<const char*>(data_.data() + off), end - off};
    }
    std::size_t size() const { return data_.size(); }
private:
    std::span<const std::byte> data_;
};

std::pair<std::uint64_t, std::size_t> uleb(const View& data, std::size_t off) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 10; ++i) {
        const auto current = data.u8(off, "export trie ULEB128");
        ++off;
        if (i == 9 && current > 1) throw FormatError("export trie ULEB128 overflow");
        value |= std::uint64_t(current & 0x7f) << (7 * i);
        if ((current & 0x80) == 0) return {value, off};
    }
    throw FormatError("export trie ULEB128 overflow");
}
}

std::vector<std::string> parse_chained_imports(std::span<const std::byte> bytes,
                                               std::uint32_t offset, std::uint32_t size) {
    View image(bytes);
    image.require(offset, size, "chained fixup payload");
    if (size == 0) return {};
    const View data(bytes.subspan(offset, size));
    data.require(0, 28, "chained fixup header");
    if (data.u32(0, "fixup version") != 0) throw FormatError("unsupported chained fixup version");
    const auto starts = data.u32(4, "fixup starts offset");
    const auto imports = data.u32(8, "fixup imports offset");
    const auto symbols = data.u32(12, "fixup symbols offset");
    const auto count = data.u32(16, "fixup import count");
    if (count > 100000) throw FormatError("chained import count exceeds safety limit");
    const auto format = data.u32(20, "fixup imports format");
    const auto symbol_format = data.u32(24, "fixup symbols format");
    if (format < 1 || format > 3) throw FormatError("unsupported chained imports format");
    if (symbol_format != 0) throw FormatError("unsupported chained symbol compression");
    data.require(starts, 4, "fixup segment starts");
    const auto segments = data.u32(starts, "fixup segment count");
    if (segments > (data.size() - starts - 4) / 4) {
        throw FormatError("chained segment offsets out of bounds");
    }
    for (std::uint32_t i = 0; i < segments; ++i) {
        const auto segment_offset = data.u32(starts + 4 + std::size_t(i) * 4, "segment start offset");
        if (segment_offset) {
            const auto at = std::uint64_t(starts) + segment_offset;
            if (at > data.size()) throw FormatError("chained segment start out of bounds");
            data.require(static_cast<std::size_t>(at), 22, "chained segment header");
            const auto header_size = data.u32(static_cast<std::size_t>(at), "chained segment size");
            if (header_size < 22) throw FormatError("chained segment header too small");
            data.require(static_cast<std::size_t>(at), header_size, "chained segment data");
            const std::uint32_t page_count =
                std::uint32_t(data.u8(static_cast<std::size_t>(at) + 20, "page count")) |
                (std::uint32_t(data.u8(static_cast<std::size_t>(at) + 21, "page count")) << 8);
            if (page_count > (header_size - 22) / 2) {
                throw FormatError("chained segment pages exceed header");
            }
        }
    }
    const std::size_t entry_size = format == 1 ? 4 : format == 2 ? 8 : 16;
    if (imports > data.size() || count > (data.size() - imports) / entry_size) {
        throw FormatError("chained import table out of bounds");
    }
    if (symbols > data.size()) throw FormatError("chained symbol pool out of bounds");
    std::vector<std::string> names;
    names.reserve(count);
    std::size_t total_name_bytes = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = std::size_t(imports) + std::size_t(i) * entry_size;
        const std::uint32_t name_offset = format == 3 ?
            static_cast<std::uint32_t>(data.u64(at, "import descriptor") >> 32) :
            data.u32(at, "import descriptor") >> 9;
        const auto absolute = std::uint64_t(symbols) + name_offset;
        if (absolute >= data.size()) throw FormatError("chained import name offset out of bounds");
        auto name = data.string(static_cast<std::size_t>(absolute), "chained import name");
        if (name.size() > 8 * 1024 * 1024 - total_name_bytes) {
            throw FormatError("chained import names exceed safety limit");
        }
        total_name_bytes += name.size();
        names.push_back(std::move(name));
    }
    return names;
}

std::vector<Export> parse_exports(std::span<const std::byte> bytes,
                                       std::uint32_t offset, std::uint32_t size) {
    View image(bytes);
    image.require(offset, size, "export trie payload");
    if (size == 0) return {};
    const View data(bytes.subspan(offset, size));
    struct Pending { std::size_t offset; std::string prefix; };
    std::vector<Pending> pending{{0, ""}};
    std::unordered_set<std::size_t> visited;
    std::vector<Export> exports;
    while (!pending.empty()) {
        auto node = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(node.offset).second) throw FormatError("export trie contains a cycle or shared node");
        if (visited.size() > 65536) throw FormatError("export trie exceeds node safety limit");
        auto [terminal_size, cursor] = uleb(data, node.offset);
        if (terminal_size > data.size() - cursor) throw FormatError("export terminal out of bounds");
        const auto terminal_end = cursor + static_cast<std::size_t>(terminal_size);
        if (terminal_size) {
            const auto [flags, next] = uleb(data, cursor);
            if (next > terminal_end) throw FormatError("export flags outside terminal");
            std::uint64_t address = 0;
            if ((flags & 0x08) == 0) {
                const auto [value, end] = uleb(data, next);
                if (end > terminal_end) throw FormatError("export address outside terminal");
                address = value;
                if ((flags & 0x10) != 0) {
                    const auto [resolver, resolver_end] = uleb(data, end);
                    static_cast<void>(resolver);
                    if (resolver_end > terminal_end) {
                        throw FormatError("export resolver outside terminal");
                    }
                }
            } else {
                const auto [ordinal, end] = uleb(data, next);
                static_cast<void>(ordinal);
                if (end >= terminal_end) throw FormatError("export reexport name missing");
            }
            exports.push_back({node.prefix, address, flags});
        }
        cursor = terminal_end;
        const auto child_count = data.u8(cursor++, "export trie child count");
        for (std::uint32_t i = 0; i < child_count; ++i) {
            const auto edge = data.string(cursor, "export trie edge");
            cursor += edge.size() + 1;
            const auto [child_offset, end] = uleb(data, cursor);
            cursor = end;
            if (child_offset >= data.size()) throw FormatError("export trie child offset out of bounds");
            if (node.prefix.size() + edge.size() > 4096) throw FormatError("export trie path too long");
            pending.push_back({static_cast<std::size_t>(child_offset), node.prefix + edge});
        }
    }
    return exports;
}
}
