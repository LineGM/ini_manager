# Contributing to ini_manager 1.1.0

Read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) and the setup instructions in
[HACKING.md](HACKING.md).

Preserve header-only integration, explicit errors, independent value semantics
and the documented parse/write contract. Add behavior-level regression tests for
fixes and update README when public behavior changes. Check CTest, examples,
clang-tidy, clang-format and spelling; run sanitizers when available. Report the
actual toolchains and platforms tested and any unavailable checks.

Shared settings belong in CMakePresets.json. Personal paths and overrides belong
in the ignored CMakeUserPresets.json. Keep downloaded dependencies, generated
documentation, packages and fuzz corpora in the build tree. Do not apply developer
flags or analysis targets to projects consuming the library.

Document current behavior and supported usage. API changes need tests for their
error handling, lifetime guarantees and effects on serialized configuration.

Use [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) for
small, focused changes: `fix(parser): reject invalid headers`,
`perf(api): avoid copying string rvalues`, `test(io): cover failed replacement`,
`build(cmake): isolate developer settings`, `ci: verify packages`, or
`docs: clarify stream ownership`. A compatibility break requires `!` in the
subject and a `BREAKING CHANGE:` footer describing the observable change.
Do not rewrite published commits or move published release tags.

Explain ownership at each `std::move`/`std::forward` site. Return eligible local
variables directly; distinguish implicit move from optional NRVO and from moving
members or values contained in `expected`. Prefer Rule of Zero unless documented
postconditions or exception guarantees require special members. Apply `noexcept`
only when every operation on that path satisfies it. Measure performance claims
on identical workloads; retain raw results and tool versions.
