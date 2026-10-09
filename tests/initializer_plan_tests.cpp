#include <anyios/initializer_plan.hpp>
#include <anyios/process_bootstrap.hpp>

#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using anyios::loader::MappedInitializerImage;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void rejected(const std::function<void()>& call) {
    try { call(); } catch (const anyios::macho::FormatError&) { return; }
    throw std::runtime_error("invalid initializer graph accepted");
}
MappedInitializerImage image(std::string path, std::uint64_t base, bool main = false) {
    anyios::macho::Image meta;
    meta.file_type = main ? 2 : 6;
    meta.segments.push_back({"__TEXT", 0x100000000ULL, 0x4000, 0, 0x4000, 1, 5, 5});
    meta.segments.push_back({"__DATA", 0x100004000ULL, 0x4000, 0x4000, 0x4000, 0, 3, 3});
    meta.sections.push_back({"__init_offsets", "__TEXT", 0x100000080ULL, 4, 0x80, 0, false});
    return {{std::move(path), std::move(meta)}, base};
}
}

int main() {
    try {
        anyios::cpu::GuestMemory memory(0x10000, 0x80000);
        const auto r = anyios::cpu::bits(anyios::cpu::Access::read);
        std::array<MappedInitializerImage, 4> inputs{{
            image("Bundle/Main", 0x10000, true),
            image("Bundle/Frameworks/Right", 0x20000),
            image("Bundle/Frameworks/Leaf", 0x30000),
            image("Bundle/Frameworks/Left", 0x40000)
        }};
        for (const auto& input : inputs) {
            check(memory.map_ios(input.guest_text_base, 0x4000, r | anyios::cpu::bits(anyios::cpu::Access::execute)), "map text");
            check(memory.map_ios(input.guest_text_base + 0x4000, 0x4000, r | anyios::cpu::bits(anyios::cpu::Access::write)), "map data");
            std::array<std::byte, 4> offset{std::byte{0}, std::byte{1}, std::byte{0}, std::byte{0}};
            check(memory.load(input.guest_text_base + 0x80, offset), "load descriptor");
            std::array<std::byte, 4> ret{std::byte{0xc0},std::byte{3},std::byte{0x5f},std::byte{0xd6}};
            check(memory.load(input.guest_text_base + 0x100, ret), "load constructor");
        }
        inputs[0].module.image.dependencies = {{"@executable_path/Frameworks/Left"}, {"@executable_path/Frameworks/Right"}};
        inputs[1].module.image.dependencies = {{"@loader_path/Leaf"}};
        inputs[3].module.image.dependencies = {{"@loader_path/Leaf"}};
        const auto plan = anyios::loader::plan_owned_image_initializers(inputs, "Bundle/Main", memory);
        check(plan.size() == 4, "shared diamond dependency must initialize once");
        const std::array<std::uint64_t, 4> expected{0x30100, 0x40100, 0x20100, 0x10100};
        for (std::size_t i = 0; i < expected.size(); ++i)
            check(plan[i].guest_function == expected[i], "dependency-first constructor order");
        auto saved = inputs;
        inputs[2].module.path = "changed";
        check(plan[0].module_path == "Bundle/Frameworks/Leaf", "plan paths must own snapshots");
        inputs = saved;
        auto validate = [&] { (void)anyios::loader::plan_owned_image_initializers(inputs, "Bundle/Main", memory); };
        inputs[0].module.image.dependencies.push_back({"@loader_path/missing"}); rejected(validate); inputs = saved;
        inputs[0].module.image.dependencies.push_back({"@loader_path/missing", true}); rejected(validate); inputs = saved;
        inputs[2].module.image.dependencies.push_back({"@executable_path/Main"}); rejected(validate); inputs = saved;
        inputs[0].module.image.dependencies[0].upward = true; rejected(validate); inputs = saved;
        inputs[0].module.image.dependencies[0].reexport = true; rejected(validate); inputs = saved;
        inputs[0].module.image.dependencies.erase(inputs[0].module.image.dependencies.begin()); rejected(validate); inputs = saved;
        inputs[1].module.path = inputs[2].module.path; rejected(validate); inputs = saved;
        inputs[1].guest_text_base = inputs[2].guest_text_base; rejected(validate); inputs = saved;
        inputs[1].guest_text_base = UINT64_MAX - 0x3fff; rejected(validate); inputs = saved;
        inputs[1].module.image.segments[1].vm_address = UINT64_MAX; rejected(validate); inputs = saved;
        inputs[1].guest_text_base = 0x60000; rejected(validate); inputs = saved;
        inputs[1].guest_text_base += 4; rejected(validate); inputs = saved;
        inputs[1].module.image.file_type = 2; rejected(validate); inputs = saved;
        inputs[1].module.image.is_encrypted = true; rejected(validate); inputs = saved;
        inputs[1].module.image.sections[0].address += 1; rejected(validate); inputs = saved;
        inputs[1].module.image.sections[0].zero_fill = true; rejected(validate); inputs = saved;
        inputs[1].module.image.sections[0].address = 0x100008080ULL; rejected(validate); inputs = saved;
        inputs[1].module.image.sections.push_back(inputs[1].module.image.sections[0]); rejected(validate); inputs = saved;
        inputs[1].module.image.sections[0].size = 65 * 4; rejected(validate); inputs = saved;
        // An already mapped other image's RX page must not legitimize a pointer.
        inputs[1].module.image.sections[0] = {"__mod_init_func", "__DATA", 0x100004080ULL, 8, 0x4080, 0, false};
        check(memory.write(0x24080, 0x30100, 8), "cross-image descriptor setup"); rejected(validate);
        check(memory.write(0x24080, 0x24100, 8), "nonexecuting descriptor setup"); rejected(validate);
        check(memory.write(0x24080, 0x20101, 8), "unaligned descriptor setup"); rejected(validate);
        check(memory.write(0x24080, 0x20100, 8), "owned pointer descriptor setup"); validate();
        inputs = saved;
        // File-backed code bounds are stricter than page-rounded RX mappings.
        inputs[1].module.image.segments[0].file_size = 0x100; rejected(validate);
        check(memory.read(0x10080, 4) == 0x100, "planning must not mutate original descriptors");
        std::cout << "Owned multi-image initializer graph ordering and refusal contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
