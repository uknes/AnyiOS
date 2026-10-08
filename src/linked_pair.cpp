#include <anyios/linked_pair.hpp>
#include <anyios/import_resolver.hpp>
#include <anyios/linked_image.hpp>
#include <anyios/macho.hpp>

#include <algorithm>
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
    cpu::GuestMemory::MappingJournal journal(memory);
    const LinkedImageOptions options{true, &journal};
    const auto library_result = stage_linked_image(library, memory, library_base, {}, options);
    const auto executable_result = stage_linked_image(
        executable, memory, executable_base, resolved, options);
    if (!executable_result.guest_entry ||
        !memory.fetch(resolved[0]).has_value()) {
        throw macho::FormatError("owned cross-library call target is not executable");
    }
    journal.commit();
    return {executable_result.guest_entry, resolved[0],
            executable_result.patched_pointers, library_result.patched_pointers};
}
}
