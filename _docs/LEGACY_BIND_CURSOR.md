# Legacy bind cursor arithmetic

The original UIKitCatalog debug dylib was read from the unchanged, SHA-256
verified bundle in PR #34's intake. Its eager bind decoder stopped with
`legacy bind address overflow`. No debug-dylib staging or execution occurred;
the unchanged main still stopped at instruction 212 in dlsym.

AnyiOS applied checked address addition to both absolute addresses and the
segment-relative opcode cursor. Apple's public dyld `BindOpcodes::forEachBind`
uses an unsigned 64-bit cursor: ADD_ADDR_ULEB adds its operand modulo 2^64,
DO_BIND_ADD_ADDR_ULEB adds the operand plus pointer size modulo 2^64, and
repeated binds use the same modular stride. Encoded large unsigned deltas
can move this relative cursor backward. dyld validates emitted sites against
the selected segment, not an unused cursor after the final binding.

AnyiOS now applies modular arithmetic only to that relative cursor. Every
emitted pointer still requires a valid segment index, aligned eight-byte
storage inside both declared file/VM extents, writable nonexecutable
permissions and bounded original file storage. File offsets and absolute VM
addresses retain checked addition; ULEB overflow/truncation, invalid ordinals,
duplicate sites, unsupported opcodes and resource caps still fail. No guest
address, binding target or relocation addend receives permission to wrap.

`inspect_legacy_eager_bind_stream` retains owned sites and reports the number
of cursor wraps and the first original opcode offset/before/delta/pointer-step/
after values. The bundle probe prints this bounded evidence if eager decoding
succeeds. Existing site-only consumers use the same decoder without changing
their API. Lazy and weak grammars retain their existing validation; lazy
streams still forbid cursor-arithmetic opcodes.

Regression tests cover backward ADD_ADDR and combined bind/advance, descending
repeated bindings, final unconsumed cursor state, diagnostic operands, original
source preservation, invalid wrapped sites and absolute VM overflow. Native
Windows/Linux ARM64 CI runs the bind contracts alongside bundle transaction
tests. Original retries are required before claiming this fixes their exact
failure; another malformed/unsupported stream remains an explicit blocker.
This does not implement weak coalescing, original graph binding, initializers,
UIKit rendering or native original-app execution. Inventories remain unchanged.

Primary source for ABI semantics (independently authored implementation):
[Apple public dyld BindOpcodes.cpp](https://github.com/apple-oss-distributions/dyld/blob/main/mach_o/BindOpcodes.cpp),
`forEachBind` cursor updates and `valid` site validation. See also
[bundle intake boundaries](BUNDLE_DEPENDENCY_INTAKE.md).
