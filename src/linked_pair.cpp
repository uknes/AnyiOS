#include <anyios/linked_pair.hpp>
#include <anyios/import_resolver.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace anyios::loader {

OwnedLinkedPair stage_owned_linked_pair(
    std::span<const std::byte> executable,
    std::span<const std::byte> library,
    cpu::GuestMemory& memory,
    std::uint64_t executable_base,
    std::uint64_t library_base) {

    const auto main_image = macho::inspect(executable);
    const auto dylib_image = macho::inspect(library);
    if (main_image.file_type != 2 || dylib_image.file_type != 6) {
        throw macho::FormatError("owned linked pair requires MH_EXECUTE and MH_DYLIB");
    }
    if (dylib_image.install_name != "@rpath/libRuntimeWidget.dylib") {
        throw macho::FormatError("unexpected owned dylib install name");
    }
    const auto dependency = std::find_if(main_image.dependencies.begin(),
        main_image.dependencies.end(),
        [](const macho::Dependency& item) {
            return item.install_name == "@rpath/libRuntimeWidget.dylib";
        });
    if (dependency == main_image.dependencies.end()) {
        throw macho::FormatError("owned executable does not depend on fixture dylib");
    }
    if (main_image.chained_imports.size() != 1 ||
        main_image.chained_imports[0] != "_anyios_widget") {
        throw macho::FormatError("owned executable contains unsupported imports");
    }
    if (!dylib_image.chained_imports.empty()) {
        throw macho::FormatError("owned dylib requires unsupported guest imports");
    }

    const auto text = std::find_if(dylib_image.segments.begin(),
        dylib_image.segments.end(),
        [](const macho::Segment& item) { return item.name == "__TEXT"; });
    if (text == dylib_image.segments.end() || library_base < text->vm_address) {
        throw macho::FormatError("invalid owned dylib slide");
    }
    const std::uint64_t slide = library_base - text->vm_address;
    const dyld::LoadedDylib registered{&dylib_image, slide};
    const auto resolved = dyld::resolve_chained_import_targets(
        executable, main_image, std::span<const dyld::LoadedDylib>(&registered, 1));
    if (resolved.size() != 1) {
        throw macho::FormatError("owned dependency did not resolve exactly one symbol");
    }
    bool executable_target = false;
    for (const auto& segment : dylib_image.segments) {
        if (!(segment.init_protection & 4u) || segment.vm_address > UINT64_MAX - slide) continue;
        const auto begin = segment.vm_address + slide;
        if (resolved[0] >= begin && resolved[0] - begin <= segment.vm_size &&
            segment.vm_size - (resolved[0] - begin) >= 4 && resolved[0] % 4 == 0) {
            executable_target = true;
        }
    }
    if (!executable_target) throw macho::FormatError("owned cross-library call target is not executable");
    const std::array<LinkedImageInput, 2> inputs{{
        {library, library_base, {}}, {executable, executable_base, resolved}
    }};
    const auto staged = stage_linked_images(inputs, memory, true);
    return {staged[1].guest_entry, resolved[0],
            staged[1].patched_pointers, staged[0].patched_pointers};
}
}
