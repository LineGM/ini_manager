# Contributing to ini_manager 1.0.0

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
