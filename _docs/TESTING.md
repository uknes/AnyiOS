# Testing

Build and test with CMake 3.20+ and C++20:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

For a Clang sanitizer build, configure with compiler clang++ and compile/link flags -fsanitize=address,undefined -fno-omit-frame-pointer, then run the same CTest suite.

Tests cover: valid ARM64 header and load commands; invalid magic, CPU and counts; command size/alignment and region bounds; segment file/vm arithmetic overflow and section bounds; dylib/rpath offsets and NUL termination; duplicate entry; iOS deployment and build-tool validation; encrypted-region bounds; deterministic 3,000-case mutation smoke loop; CLI help.

These are original synthetic fixtures with no Apple copyrighted assets. Mutation smoke tests are not coverage-guided fuzzing. Passing parser tests does not mean an iOS game will run. Each bug fix must add a reproducer and preserve explicit failure diagnostics.
