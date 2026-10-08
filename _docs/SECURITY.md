# Security and interoperability

Treat binary input as malicious until proven otherwise. Validate every command count, offset, length, string, VM span and multiplication prior to indexing or allocating. The CLI reads at most 1 GiB and never runs inspected code.

A nonzero encrypted-code indicator is reported with a distinct exit status. The inspector does not decrypt protected code.

Do not commit or redistribute proprietary IPAs, firmware, SDK libraries, cryptographic keys, leaked documentation, decrypted commercial games or DRM circumvention tools. Use user-owned and explicitly redistributable fixtures.

Before any actual code execution, review executable page protections, process isolation, memory maps, syscall boundaries, entitlements, and host privileges. Report security issues with minimal non-exploitable reproductions until a private maintainer channel is configured.
