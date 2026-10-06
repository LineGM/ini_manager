# Developing ini_manager 1.0.0

Use CMake 4.0+ and a supported C++26 toolchain from README. Shared presets live in
`CMakePresets.json` (schema 10); `CMakeUserPresets.json` is ignored and reserved for
local paths and overrides.

## Build and verify

```sh
cmake --workflow --preset=verify-gcc
cmake --workflow --preset=verify-clang
```

The GCC workflow configures, builds, runs CTest and the examples, and compiles
the public header independently through `all_verify_interface_header_sets`.
The Clang workflow also runs clang-tidy, clang-format and spelling checks.
Choose `dev-darwin` for Homebrew GCC on macOS. Windows is excluded until a
toolchain satisfies the full numeric and C++26 library contract; see README.
Individual steps remain available:

```sh
cmake --preset=dev-linux-clang
cmake --build --preset=dev-linux-clang --parallel 2
ctest --preset=dev-linux-clang
cmake --build --preset=dev-linux-clang --target run-examples
```

Developer mode is active only when this repository is the top-level project.
Compiler and linker warnings are errors for project executables; this is controlled
by `INI_MANAGER_WARNINGS_AS_ERRORS`. Warning flags, sanitizer flags and coverage
instrumentation are private to those targets. Consumer targets and dependency
builds receive none of these settings. No global compiler/linker flags are changed.
The exported interface uses a `HEADERS` file set and `cxx_std_26`.

On non-Windows developer builds a compilation database link is created only if
no existing file or link occupies that path. `tidy-check` uses the selected build
directory explicitly. Module scanning is disabled on project executables because
the library is distributed as a header.

Tests require Python 3 for tooling integration checks and use a pinned Boost.UT
revision fetched into the build tree. For offline builds configure
with `-DFETCHCONTENT_SOURCE_DIR_BOOST.UT=/path/to/boost.ut`. CTest labels `unit`,
`integration`, `headers` and `package` allow focused runs. Package tests verify
add_subdirectory, local FetchContent and installed find_package consumers,
including the absence of developer settings in their builds.

## Code quality

```sh
cmake --build --preset=dev-linux-clang --target tidy-check
cmake -DFIX=YES -P cmake/lint.cmake
cmake -P cmake/lint.cmake
cmake -P cmake/spell.cmake
```

Use clang-tidy and clang-format 23 with the checked-in configuration files.
CI prints the selected versions and checks all enabled diagnostics.
`TIDY_COMMAND`, `FORMAT_COMMAND` and `SPELL_COMMAND` can select executables.
Every enabled clang-tidy diagnostic is an error, including style diagnostics.
The analysis covers all enabled project translation units and the public header;
dependency sources and generated build files are excluded. Use a Clang build to
match the compilation database to the analyzer.

Anonymous namespaces are the local-linkage convention. The contradictory
`llvm-prefer-static-over-anonymous-namespace` rule is disabled in favor of
`misc-use-anonymous-namespace`. Narrow suppressions must explain a necessary
native ABI operation or an intentional regression-test condition. Do not silence
whole files to obtain a passing check.

## Sanitizers, coverage and fuzzing

```sh
cmake --preset=ci-sanitize
cmake --build --preset=ci-sanitize --parallel 2
ctest --preset=ci-sanitize
cmake --preset=ci-coverage
cmake --build --preset=ci-coverage --parallel 2
ctest --preset=ci-coverage
cmake --build --preset=ci-coverage --target coverage
```

`INI_MANAGER_ENABLE_SANITIZERS=ON` enables ASan/UBSan on project executables.
The sanitizer preset explicitly selects `clang++`; use the same supported LLVM
toolchain as the Clang quality checks.
`ENABLE_COVERAGE=ON` uses GCC
coverage instrumentation and the `coverage` target requires lcov. Each preset has
its own build directory. LeakSanitizer cannot run under ptrace; when necessary,
use `ASAN_OPTIONS=detect_leaks=0` and report that limitation with the results.

```sh
cmake --preset=fuzz
cmake --build --preset=fuzz --parallel 2
mkdir -p build/fuzz/corpus
cp test/fuzz/corpus/* build/fuzz/corpus/
build/fuzz/ini_manager_fuzz -max_total_time=30 -max_len=65536 build/fuzz/corpus
```

The optional `INI_MANAGER_BUILD_FUZZER` target requires Clang/libFuzzer and enables
its own sanitizer instrumentation. The first input byte selects dialect options;
remaining bytes form the document. Successful parses are serialized, reparsed and
compared. Keep tracked seeds unchanged and write generated corpus inputs into the
build tree.

## Documentation and packaging

Configure with `BUILD_DOCS=ON` to enable the `docs` target. Doxygen is required;
Graphviz is optional. All generated documentation goes beneath the binary tree.
The independent CI entry point follows the same convention:

```sh
cmake -DPROJECT_SOURCE_DIR="$PWD" -DPROJECT_BINARY_DIR="$PWD/build/docs-ci" -P cmake/docs-ci.cmake
cmake --workflow --preset=package-release
```

Keep README, BUILDING and these instructions synchronized with the public API,
package requirements and actual presets. Documentation describes the supported
release and its users' and maintainers' workflows.
