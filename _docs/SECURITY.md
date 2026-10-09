# Security and interoperability

Treat binary input as malicious until proven otherwise. Validate every command count, offset, length, string, VM span and multiplication prior to indexing or allocating. The CLI reads at most 1 GiB and never runs inspected code.

A nonzero encrypted-code indicator is reported with a distinct exit status. The inspector does not decrypt protected code.

Do not commit or redistribute proprietary IPAs, firmware, SDK libraries, cryptographic keys, leaked documentation, decrypted commercial games or DRM circumvention tools. Use user-owned and explicitly redistributable fixtures.

Before any actual code execution, review executable page protections, process isolation, memory maps, syscall boundaries, entitlements, and host privileges. Report security issues with minimal non-exploitable reproductions until a private maintainer channel is configured.


## Parser resource policy

At most 4096 universal slices, 16384 thin load commands, 100000 chained imports, 16384 bytes per dyld symbol/edge string, 8 MiB aggregate chained import-name bytes and 65536 traversed export trie nodes. Numeric bounds protect against overflows; resource ceilings limit accepted workload and do not imply all accepted input is trusted. Coverage-guided fuzzing is enabled only for the parser and does not sandbox potentially executable guest code.

LC_SYMTAB has a separate one-million-entry / 64 MiB aggregate copied-name budget per image, with individual names shorter than 16 KiB and validated string-table ranges. Local/debug entries remain retained and do not become exports. Dynamic lookup retains its lower 65,536-export / 8 MiB module-name budget. See [large symbol-table policy](LARGE_SYMBOL_TABLES.md).
