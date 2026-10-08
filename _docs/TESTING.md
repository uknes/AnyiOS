# Testing

Build and test with CMake 3.20+ and C++20:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

For a Clang sanitizer build, configure with compiler clang++ and compile/link flags -fsanitize=address,undefined -fno-omit-frame-pointer, then run the same CTest suite.

Tests cover: valid ARM64 header and load commands; invalid magic, CPU and counts; command size/alignment and region bounds; segment file/vm arithmetic overflow and section bounds; dylib/rpath offsets and NUL termination; duplicate entry; iOS deployment and build-tool validation; encrypted-region bounds; deterministic 3,000-case mutation smoke loop; CLI help.

These are original synthetic fixtures with no Apple copyrighted assets. Mutation smoke tests are not coverage-guided fuzzing. Passing parser tests does not mean an iOS game will run. Each bug fix must add a reproducer and preserve explicit failure diagnostics.


## M1 tests

- FAT/FAT64 with both canonical and swapped byte orders, malformed slices, alignment and subtype mismatches.
- LC_SYMTAB symbol names, string-table bounds, forged symbol counts and LC_DYSYMTAB prerequisites; section_64 zero-fill, contents and relocation table ranges.
- Chained import table and symbol bounds, uncompressed names, export tries, cycles and ULEB128 errors.
- Host Clang compiles original ARM64 iOS source to an MH_OBJECT Mach-O in tests/real_fixture.py; both the thin object and a crafted dual-architecture wrapper are inspected. Where llvm-objdump is available, its metadata provides an independent cross-check.
- tests/fuzz_corpus.py generates four original seeds (thin, universal, chained imports, exports); tests/fuzz_macho.cpp feeds arbitrary byte spans to the same parser.
- CI fuzz target is built with -fsanitize=fuzzer,address,undefined and runs 15,000 iterations under Linux Clang.
- Verified end-to-end CI: https://github.com/uknes/AnyiOS/actions/runs/37708104543

## Fuzzing locally

    cmake -S . -B build-fuzz -DANYIOS_BUILD_FUZZER=ON -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTING=OFF
    cmake --build build-fuzz --target anyios-fuzz
    python3 tests/fuzz_corpus.py build-fuzz/corpus
    ./build-fuzz/anyios-fuzz build-fuzz/corpus -runs=10000 -max_len=4096 -timeout=3

The seed generator contains no Apple or commercial binary data. The production inspector refuses encrypted code execution because it never executes code at all.

## Windows CPU translation test

The GuestMemory page-mapping tests run in the ordinary cross-platform CTest matrix and check mapped/unmapped ranges, write-to-code rejection, data execute rejection, overlap, byte ordering and address overflows.

The optional Dynarmic A64 smoke test is enabled only with ANYIOS_WITH_DYNARMIC=ON. GitHub's dedicated Windows x86-64 job installs pinned Boost headers, fetches Dynarmic at a pinned Git revision, builds the translator and checks owned ARM64 MOVZ/RET instruction execution, x0=42 and the return PC. The guard-page test checks a fully unmapped page, not unused bytes in a mapped executable page. Passing this test does not establish Mach-O app support.
