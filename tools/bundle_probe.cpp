#include <anyios/dyld_plan.hpp>
#include <anyios/legacy_bind.hpp>
#include <anyios/macho.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
class BundleReader {
public:
    explicit BundleReader(const std::filesystem::path& root)
        : root_(std::filesystem::canonical(root)) {
        if (!std::filesystem::is_directory(root_)) throw std::runtime_error("bundle root is not a directory");
    }
    std::optional<anyios::macho::Image> operator()(std::string_view candidate) {
        constexpr std::string_view prefix = "Bundle/";
        if (!candidate.starts_with(prefix)) throw std::runtime_error("dependency escapes bundle root");
        const std::filesystem::path relative(std::string(candidate.substr(prefix.size())));
        if (relative.empty() || relative.is_absolute()) throw std::runtime_error("invalid bundle path");
        auto path = root_;
        for (const auto& component : relative) {
            if (component == ".." || component == ".") throw std::runtime_error("noncanonical bundle component");
            path /= component;
            std::error_code error;
            const auto status = std::filesystem::symlink_status(path, error);
            if (error == std::errc::no_such_file_or_directory ||
                (!error && status.type() == std::filesystem::file_type::not_found)) return std::nullopt;
            if (error) throw std::runtime_error("cannot inspect bundle dependency path");
            if (std::filesystem::is_symlink(status)) throw std::runtime_error("bundle dependency symlinks unsupported");
        }
        if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("bundle dependency is not a regular file");
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("cannot read bundle dependency");
        const auto size = file.tellg();
        constexpr std::size_t file_limit = 128U * 1024U * 1024U;
        constexpr std::size_t total_limit = 256U * 1024U * 1024U;
        if (size <= 0 || size > static_cast<std::streamoff>(file_limit) ||
            static_cast<std::uint64_t>(size) > total_limit - bytes_ || count_ >= 256) {
            throw std::runtime_error("bundle dependency file/count limit exceeded");
        }
        ++count_;
        bytes_ += static_cast<std::size_t>(size);
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("incomplete bundle dependency read");
        auto image = anyios::macho::inspect(bytes);
        if (image.is_fat || image.is_encrypted) throw std::runtime_error("bundle requires thin unencrypted ARM64 images");
        std::cout << "BUNDLE_MODULE=" << candidate << "\n"
                  << "BUNDLE_MODULE_FILE_BYTES=" << size << "\n"
                  << "BUNDLE_MODULE_INSTALL_NAME=" << image.install_name << "\n"
                  << "BUNDLE_MODULE_CHAINED_IMPORTS=" << image.chained_imports.size() << "\n";
        for (const auto& runpath : image.rpaths)
            std::cout << "BUNDLE_MODULE_RPATH=" << runpath << "\n";
        if (image.legacy_dyld) {
            const auto eager = anyios::dyld::inspect_legacy_eager_bind_stream(bytes, image);
            const auto lazy = anyios::dyld::inspect_legacy_lazy_bind_sites(bytes, image);
            const auto weak = anyios::dyld::inspect_legacy_weak_bind_sites(bytes, image);
            std::cout << "BUNDLE_MODULE_LEGACY_EAGER_BINDS=" << eager.sites.size() << "\n"
                      << "BUNDLE_MODULE_LEGACY_LAZY_BINDS=" << lazy.size() << "\n"
                      << "BUNDLE_MODULE_LEGACY_WEAK_BINDS=" << weak.sites.size() << "\n"
                      << "BUNDLE_MODULE_LEGACY_NON_WEAK_DEFINITIONS=" << weak.non_weak_definitions.size() << "\n"
                      << "BUNDLE_MODULE_LEGACY_EAGER_CURSOR_WRAPS=" << eager.cursor_wraps << "\n";
            if (eager.first_cursor_wrap) {
                const auto& wrap = *eager.first_cursor_wrap;
                std::cout << "LEGACY_CURSOR_WRAP_OPCODE_OFFSET=" << wrap.opcode_offset << "\n"
                          << "LEGACY_CURSOR_WRAP_BEFORE=" << wrap.before << "\n"
                          << "LEGACY_CURSOR_WRAP_DELTA=" << wrap.delta << "\n"
                          << "LEGACY_CURSOR_WRAP_POINTER_ADVANCE=" << wrap.pointer_advance << "\n"
                          << "LEGACY_CURSOR_WRAP_AFTER=" << wrap.after << "\n";
            }
        }
        return image;
    }
private:
    std::filesystem::path root_;
    std::size_t count_ = 0;
    std::size_t bytes_ = 0;
};
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: anyios-bundle-probe <unchanged-app-bundle> <bundle-relative-executable>\n";
        return 2;
    }
    try {
        BundleReader reader(argv[1]);
        const auto graph = anyios::dyld::discover_dependencies("Bundle/" + std::string(argv[2]),
            [&](std::string_view path) { return reader(path); });
        std::cout << "BUNDLE_DISCOVERY=metadata-only\nBUNDLE_MODULE_COUNT=" << graph.modules.size() << "\n";
        for (const auto& runpath : graph.external_runpaths) {
            std::cout << "BUNDLE_EXTERNAL_RPATH_LOADER=" << runpath.loader << "\n"
                      << "BUNDLE_EXTERNAL_RPATH=" << runpath.path << "\n";
        }
        for (const auto& issue : graph.unresolved) {
            std::cout << "BUNDLE_UNRESOLVED_LOADER=" << issue.loader << "\n"
                      << "BUNDLE_UNRESOLVED_DEPENDENCY=" << issue.install_name << "\n"
                      << "BUNDLE_UNRESOLVED_WEAK=" << issue.weak << "\n";
        }
        std::cout << "BUNDLE_DEPENDENCY_CLOSURE=" << (graph.unresolved.empty() && graph.external_runpaths.empty() ? "discovered" : "incomplete")
                  << "\nBUNDLE_STAGING=not-attempted\nBUNDLE_GUEST_EXECUTION=not-attempted\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BUNDLE_DISCOVERY=blocked\nBUNDLE_LOADER_REQUIREMENT=" << error.what()
                  << "\nBUNDLE_STAGING=not-attempted\nBUNDLE_GUEST_EXECUTION=not-attempted\n";
        return 3;
    }
}
