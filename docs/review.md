# C++26 engineering review

This is a maintainer review of the public API, header implementation, tests,
examples, CMake integration, CI and documentation. The comparison baseline is
commit `824a2acd2886aa9e0ee3d8c390ac5e70b3fa18c6`; published release artifacts
remain unchanged. Changes are on `review/cxx26-modernization`.

**Performance acceptance is pending.** Compilation, tests, strict clang-tidy,
sanitizers, fuzzing, docs and packaging passed on runners at `568c1b3`.
The benchmark infrastructure still needs a successful run and artifact review.
The acceptance record below identifies the exact run; no speedup is claimed.

## Findings and corrective changes

Locations name functions in `include/ini_manager/ini_manager.hpp` unless a path
is given. High means possible unintended configuration modification; medium
means misleading diagnostics or verification; performance items are candidates
whose benefit must be measured. No timing improvement is asserted here.

| ID / priority | Location and evidence | Change and compatibility | Regression / measurement |
| --- | --- | --- | --- |
| R1 / high | `load_file`, `write_file`: storing `config.ini` unchanged means a later working-directory change redirects an associated save to another directory's file. | Resolve paths once and store the absolute successful load path. **Breaking:** `file_path()` and file error paths can now be absolute. No canonicalization or inode tracking. | `test_loaded_path_survives_working_directory_changes`: two files, directory change, associated save, explicit relative save and failed reload. |
| R2 / medium | `error::message`: raw caller-controlled names and paths can contain newlines, NUL or terminal controls. Displayed diagnostics can be split or misleading even when the structured error is correct. | Use escaped string and C++26 path formatting; retain original structured fields. Add temporary-file context. Requires path-format support. Message text is not a stable machine-readable format. | `test_diagnostics_escape_untrusted_context` checks escaping and preservation of original context. |
| R3 / medium | `cmake/tidy.cmake`: excluding any absolute path containing `/build/` excludes a complete checkout nested under such a directory. Empty selections previously passed. Regex characters in checkout paths also distort the header filter. | Exclude the actual binary-tree prefix, escape the header-filter path, reject an empty selection. No runtime API change. | `test/consumer/check_tidy.py` checks a checkout below `build`, regex punctuation, duplicates, generated/external sources and empty selections without invoking a compiler. |
| R4 / performance | `set_value`, `string_text`: conversion through a view copies an owning string even when supplied as a consumable rvalue. | Transfer string ownership only after validation and after all required borrowed names have been used or owned. Lvalues still copy. An invalid string is not consumed. | `test_string_sinks_preserve_aliased_names` covers short/long strings and borrowed names into the source. `set_strings` and `string_defaults` workloads compare allocations and timing. |
| R5 / performance | `read_custom`: constructing `istringstream` copies the complete stored value for synchronous extraction. | Use `ispanstream` over borrowed bytes. **Contract restriction:** custom extraction must not mutate/retain the buffer, or mutate/destroy its manager reentrantly. Custom returned values must own their data. | Existing extraction/remainder/exception tests plus `test_move_only_conversions_and_defaults`; `custom_reads` measurements. |
| R6 / performance | `merge`: serializing a candidate merely to validate it allocates and fills a complete document that is immediately discarded. | Separate validation and checked canonical sizing from materialization. Merge only measures; actual serialization reserves the measured size. Two traversals during writing are an explicit tradeoff. | Canonical-size boundary regression; existing transactional merge and round-trip tests; `merge_documents` and `serialize_documents` measurements. |
| R7 / performance | `set_value`: inserting each new key recounts every section's keys; adding one key per section incurs a quadratic section traversal. | Maintain the total through insertion, removal, load, merge, copy, move and swap. Update it only after successful changes. Private object layout changes; rebuild consumers. | `test_key_limit_tracks_value_operations`; `insert_sections` measurement with 3,000 sections. |
| R8 / low | `.github/workflows/ci.yml`: package wildcard also selects CPack staging directories; docs are not built for pull requests. | Upload only distributable archives. Validate docs in PRs; retain the main-only publication condition. | Runner package artifact and PR docs job. |
| R9 / medium | `cmake/install-rules.cmake` prepopulates the shared `CMAKE_INSTALL_LIBDIR` cache with `lib`, despite never using it. With prefix `/`, a clean GNUInstallDirs configuration chooses `usr/lib`, but including the library first changes the consumer default to `lib`. | Remove the unnecessary cache write and let GNUInstallDirs select its normal default. The header/config installation layout is unchanged. | `check_install_dirs.py` reproduced the mismatch before the fix and passed afterward for `/`, `/usr` and `/usr/local`; it configures header-only fixtures without compilation or installation. |

R9's expected prefix behavior is documented by CMake's
[CMP0193](https://cmake.org/cmake/help/latest/policy/CMP0193.html). The regression
compares against the installed CMake's own GNUInstallDirs result rather than
hard-coding a platform-specific library directory.

## Ownership and move audit

The audit grouped every `std::move`/`std::forward` site and every owning return
in the public header by the following rules. Test and example uses that consume
successful `expected` results follow the same rules.

| Pattern | Decision |
| --- | --- |
| `return result;` for a same-type local owning result | Keep it. It allows optional NRVO; otherwise overload resolution applies implicit move when eligible. Do not require a particular constructor count in tests. |
| `return manager;`, `return converted;`, `return fallback;` into `expected<T, error>` | Keep the plain return. This is a converting return using implicit move, **not NRVO of a same-type object**. Move-only getter/default regressions exercise it. |
| `return std::move(m_data);` in consuming `finish() &&` | Keep it: the source is a member, not an eligible local return variable. The qualifier communicates consumption of parser state. |
| `std::move(reader).finish()` and `std::move(m_parser).finish()` | Keep them to select the consuming member function. |
| `std::move(*parsed)`, `std::move(result.error())` | Keep them: dereferencing or calling an accessor on an lvalue wrapper does not implicitly consume the contained object. Ownership passes into a candidate, return diagnostic or save operation. |
| `std::move(value)` inside merge and the string sink | Keep it after the final use of the source. Moving mapped values does not move map keys. Do not make these sources const. |
| `std::forward<T>(value)` in constrained forwarding functions | Preserve caller value category exactly once. The repeated parser source callable is copied once and called as an lvalue, not forwarded repeatedly. |
| `const auto` for read-only state and results | Keep it where no ownership is transferred; remove neither const nor copies speculatively. A const string generally cannot use its normal move constructor. |

The distinction comes from [copy elision](https://eel.is/c++draft/class.copy.elision)
and [move-eligible expressions](https://eel.is/c++draft/expr.prim.id.unqual).
[P2266R3](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2266r3.html)
is the adopted C++23 simplification, not a new C++26 optimization.

Rule of Zero remains the default recommendation. The manager intentionally has
custom special members: copy assignment must commit all state together, and move
must leave the source empty with default options and no path. Default memberwise
assignment would not provide those postconditions. Move's exception specification
accounts for empty-map construction; swap uses fixed standard allocators and a
nothrow comparator. RAII file handles and cleanup guards cannot be copied.

`test_self_assignment_preserves_data_and_options` covers aliasing assignments.
Compile-time assertions check swap and conditional move exception specifications.
There is no test assuming that optional NRVO happens. Allocation, formatting and
custom extraction remain potentially throwing operations.

The supplied [article on value categories](https://habr.com/ru/articles/985066/)
was considered as explanatory reading, not as normative evidence. Its advice
about avoiding pessimizing returns is useful. Its blanket statements about
moved-from objects and vector relocation need qualification: standard-library
objects retain valid states subject to their contracts, and a non-copyable
element can still require a potentially throwing move. See
[moved-from library types](https://eel.is/c++draft/lib.types.movedfrom) and
[vector exception guarantees](https://eel.is/c++draft/vector.modifiers).
Our explicit empty-source manager contract is stronger than a generic rule.

## API, parser and file guarantees retained

- Owning results and transparent ordered maps avoid escaped internal references.
  Borrowed section/key wrappers are only valid during a call. Setter aliasing is
  checked before moving the supplied string, including small-string storage.
- Concepts reject views, pointers and reference types as getter results, accept
  move-only custom extraction, and separate string defaults from typed defaults.
  Finding malformed data still returns an error rather than applying a default.
- The parser bounds byte accumulation before buffer growth. Canonical sizing
  uses subtraction before additions, maintaining the total below its configured
  limit. Count checks precede insertion. Limits bound data, not wall time or
  process-wide memory; files and streams can block.
- Syntax parsing remains separate from storage commit. Failed load/merge leaves
  data, options and path unchanged. The maintained key count participates in
  every commit and swap. Caller streams are not rewound on failure.
- Integer/floating conversion requires complete consumption, uses no global
  locale, rejects non-finite values, and preserves supported subnormal values
  and signed zero. Existing range and locale tests remain acceptance checks.
- Stream methods distinguish EOF from known failures, preserve previous error
  flags and exception masks, and convert stream-buffer failures to diagnostics.
  User conversion exceptions and `bad_alloc` propagate. Normal EOF can still
  trigger an operator exception when the caller requests eofbit exceptions.
- Serialization validates the complete configuration before destination access.
  Native exclusive creation, checked writes/close and a single replacement are
  preserved. Cleanup failure does not overwrite the initial diagnostic.
- A path is not a filesystem lock. Absolute paths protect against later cwd
  changes; trusted, stable directories are still required. Concurrent saves can
  overwrite each other's updates. There is no durability claim or fsync.
- Concurrent const manager access requires well-behaved callbacks. Mutation,
  moving and destruction require caller synchronization. No mutex or hidden
  shared configuration state is introduced.

Existing generated round-trip cases, faulting stream buffers, injectable native
save failures, parser corpus, numeric boundaries, standalone-header and multiple
translation-unit tests remain necessary. Inspection has not identified another
confirmed correctness defect in those paths; that is not a proof of absence.

## C++26 decisions

| Facility | Status and decision |
| --- | --- |
| Heterogeneous map insertion, P2363R5 | Already used; retain it with transparent comparators and feature checks. It avoids temporary owning lookup keys. |
| Boolean charconv result testing, P2497R0 | Already used; retain concise checked conversion results. This does not replace full-consumption/range checks. |
| Filesystem path formatting, P2845R8 | Adopted for diagnostic context, with debug escaping and a required feature macro. Avoid locale-sensitive intermediate path-to-string conversion. |
| Deleted functions with reasons, P2573R2 | Use in the move-only test fixture to explain its intentionally unavailable copy operations. Supported by the selected compiler generation; not a runtime feature. |
| `ispanstream` | Adopted C++23 library facility, usable in C++26. It borrows a bounded read-only range; explicit callback lifetime constraints accompany adoption. |
| Expanded constexpr support | Keep pure byte validators constexpr and test them at compile time. Do not turn runtime I/O or the whole mutable configuration API into compile-time machinery without a use case. |
| Constraints, pack indexing, structured-binding packs | No variadic dispatch problem here warrants extra machinery. Existing constrained overloads and ordinary structured bindings are clearer. |
| Ranges and range adaptations | Keep simple non-owning algorithms over immediate inputs. Do not return lazy pipelines into manager storage or replace clear loops mechanically. |
| Reflection / expansion statements | C++26 facilities, but not implemented sufficiently across this project's GCC/Clang analysis matrix. No current schema-binding API needs them; defer. |
| Contracts | C++26 facility with incomplete selected-tool support. Invalid INI and failed I/O are expected external errors and must remain `expected`, not contract violations. No adoption. |
| SIMD | C++26 library facility; byte parsing has sequential syntax and diagnostics. No profiling evidence yet justifies a second vectorized scanner. Defer rather than introduce an experimental SIMD dependency. |
| `inplace_vector`, `hive`, `function_ref`, `copyable_function` | Different storage/callback use cases; none replaces ordered owning configuration maps or the simple templated reader callable usefully. |
| Modules, coroutines, execution framework | No demonstrated improvement to a synchronous header-only parser. Separate module distribution and asynchronous I/O are outside this API. |
| Trivial relocation | Do not assume proposals or compiler extensions permit byte-copying strings/maps. The implementation transfers ownership through standard operations. |

The [GCC status](https://gcc.gnu.org/projects/cxx-status.html) and
[Clang status](https://clang.llvm.org/cxx_status.html) tables distinguish accepted
language changes from implementation support. Standard-library support is
separate. The public-header feature probes, numeric run probe, strict analysis
and supported-platform CI are the executable acceptance criteria. A language
mode flag alone is insufficient. Windows stays outside the supported matrix.

Useful primary references also include
[Core Guidelines F.18/F.19/F.48 and C.20/C.66](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines),
[P2845R8](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2845r8.html),
[span-stream construction](https://eel.is/c++draft/ispanstream.cons), and
[escaped formatting](https://eel.is/c++draft/format.string.escaped).

## Measurements and acceptance record

[Run 37576969951](https://github.com/LineGM/ini_manager/actions/runs/37576969951)
on `568c1b3` passed every job except benchmarks. In particular, the Linux Clang
job passed formatting, spelling and strict tidy; the sanitizer job passed CTest,
ASan/UBSan with leak detection enabled, and the bounded fuzz run. macOS passed
the directory-alias regression. Docs were built without publishing from the PR.
The benchmark's Git setup was corrected. In
[run 37578585244](https://github.com/LineGM/ini_manager/actions/runs/37578585244),
both benchmark executables compiled, but Valgrind could not initialize its
mandatory loader `memcmp` redirection without glibc debug symbols. The workflow
now fetches matching loader symbols through Arch debuginfod and checks Valgrind
before measuring. Successful allocation measurements and analysis remain pending.

The optional benchmark target is part of Clang's compilation database and strict
analysis. `test/benchmark/compare.py` builds identical public-API workloads against
the fixed baseline and current header on a GitHub runner. It records compiler
identity, flags, commit IDs, compile/link wall time, executable size, alternating
native timing samples, checksums and separate Valgrind allocation counts/bytes.
Allocation counts are not constructor-copy counts. Setters, defaults and getters
have explicit input/setup costs in both versions; see HACKING for measurement
scope. Short timing samples and shared-runner noise can invalidate small deltas.

| Check | Current evidence |
| --- | --- |
| Tooling selection regression | Passed locally without a C++ compiler or real analyzer. |
| Consumer install-directory defaults | Failed before the fix and passed afterward using compiler-free CMake configure comparisons. |
| Formatting, whitespace and Python syntax | Local checks passed; formatting and spelling also passed in run 37576969951. |
| GCC / Clang / macOS builds, tests and examples | Passed in run 37576969951, including independent header compilation. |
| Strict clang-tidy for all enabled project translation units | Passed in run 37576969951, including benchmark, sanitizer and fuzz targets. |
| ASan / UBSan / LeakSanitizer and parser fuzzing | Passed in run 37576969951 with leak detection enabled. No local sanitizer result is claimed. |
| Consumer integration, docs and distributable archives | Tests, documentation build and package job passed in run 37576969951. |
| Before/after timing, allocations and compilation cost | Harness prepared; no result or speedup claimed yet. |

CI is not polled continuously. Review its completed results and measurement
artifacts on the maintainer's signal before accepting performance claims or
declaring this review complete. Keep changes as Conventional Commits and leave
the published release untouched.
