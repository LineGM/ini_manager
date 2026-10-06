# ini_manager 1.0.0

A small header-only C++26 INI library with independent copies, strict parsing,
transactional loading and checked file replacement. Include
`<ini_manager/ini_manager.hpp>` and use `ini::ini_manager`.
No third-party runtime libraries are required.

## Requirements and integration

C++26 mode is required, with these **standard library** features:

- `std::expected`, `std::formattable`/`std::format`;
- span-based input streams (`std::ispanstream`);
- integer and floating-point `from_chars`/`to_chars`, including `long double`;
- heterogeneous map insertion (P2363R5,
  `__cpp_lib_associative_heterogeneous_insertion >= 202306L`);
- boolean testing of charconv results (P2497R0, `__cpp_lib_to_chars >= 202306L`);
- filesystem path formatting (P2845R8, `__cpp_lib_format_path >= 202403L`).

The C++26 insertions accept borrowed `string_view` names directly: replacing an
existing key does not first allocate temporary section/key strings. Stored names
still own their memory. Charconv results are checked before values are used.
The API uses C++26; the features listed above are required.

The supported baseline is GCC 16/libstdc++16 or Clang 22/libstdc++16.
CI selects these on Linux and Homebrew GCC 16 on macOS.
Windows currently has no supported toolchain: MinGW/libstdc++16 narrows
`from_chars(long double)` through `double`, while MSVC/libc++ do not provide all
required library features. Windows is excluded from the supported CI matrix;
its native file adapter remains available for future compatible toolchains.
Apple Clang/libc++ is also outside the supported matrix.
Other toolchains must pass the feature probe; selecting `-std=c++26` alone does
not establish standard-library support. The numeric contract includes `long double`; it is not silently
replaced with `double` or locale-dependent stream conversion.

Developer builds compile and link a probe of the actual public API, then run
numeric boundary round-trips on native builds (or with a cross-compiling emulator).
The standalone
header also diagnoses missing C++26 mode and feature-test macros. C++26 compiler
support is still evolving: use the toolchains above, and keep compiler and standard
library versions together.

Strict project analysis uses LLVM 23 clang-tidy/clang-format. CI installs rolling
Linux packages and records actual tool versions; the requirements above are
capabilities, not a promise that every package with a matching version works.

File I/O provides POSIX (including Linux/macOS) and Windows adapters. It uses a
small native adapter for exclusive creation, reliable system error codes, and
explicit closing. This is still one header; no compiled library is shipped.

CMake 4.0 or newer:

```cmake
add_subdirectory(path/to/ini_manager)
target_link_libraries(my_app PRIVATE ini_manager::ini_manager)
```

Alternatively use `FetchContent_Declare` with this repository and a pinned
revision, or install the project and use `find_package(ini_manager 1.0 REQUIRED)`.
All forms expose the same target. Consumer builds do not download tests, change
global compiler flags, add developer targets, or create compilation database links.
See [BUILDING.md](BUILDING.md) and [HACKING.md](HACKING.md).

## Checked API

```cpp
#include <ini_manager/ini_manager.hpp>
#include <iostream>

int main() {
    auto loaded = ini::ini_manager::from_file("app.ini");
    if (!loaded) {
        std::cerr << loaded.error().message() << '\n';
        return 1;
    }
    auto config = std::move(*loaded);

    auto port = config.get_value<unsigned>({"server"}, {"port"});
    if (!port) {
        std::cerr << port.error().message() << '\n';
        return 1;
    }
    auto host = config.get_value_or_default(
        {"server"}, {"host"}, "localhost");
    if (!host) {
        std::cerr << host.error().message() << '\n';
        return 1;
    }
    if (auto changed = config.set_value({"server"}, {"port"}, 9000); !changed) {
        std::cerr << changed.error().message() << '\n';
        return 1;
    }
    if (auto saved = config.write_file(); !saved) {
        std::cerr << saved.error().message() << '\n';
        return 1;
    }
}
```

`from_stream`, `load_stream`, `add_from_stream`, and `write_stream` take standard
streams. File counterparts take `std::filesystem::path`, including native Unicode
paths on Windows. A successful `load_file` associates its path; `load_stream`
clears the association. Merge and explicit `write_file(path)` do not change it.
`file_path()` returns an owning `optional<filesystem::path>`. File operations
resolve relative paths to absolute paths once. A successful file load stores that
absolute path, so changing the working directory does not redirect `write_file()`.
This does not canonicalize symlinks or track files across directory renames.

`set_section({"empty"})` preserves a section without keys. `remove_value(section,
key)` and `remove_section(section)` return whether anything was removed. Removing
the last key leaves its section. `get_sections()` and `get_keys(section)` return
sorted owning vectors; an absent section gives an empty key list.

## Ownership and errors

Maps are stored directly. Copy construction/assignment creates independent data;
move operations leave the source empty, with default options and no associated
path. Moving is conditionally noexcept because some standard libraries allocate
an empty map sentinel. Copy assignment, load, and merge commit only after successful preparation.
Self-copy and self-move assignment preserve the object, including its options.
Parse, read, allocation, and merge failures preserve the previous manager state.
A failed setter also preserves its data. Reading from an input stream is not
rewound on failure; output streams can contain a partial write on failure.

There are no section accessors, mutable references, iterators or views into the
manager. Returned values remain valid after reload, modification, move or owner
destruction. Getters on temporary managers are safe because they return owned
values. `section` and `key` are **borrowed call arguments**, wrapping `string_view`;
keep their source strings alive until the call returns. Do not retain a wrapper
constructed from a temporary string. Managers never retain argument views.

String rvalues passed to setters or a needed default can transfer their storage;
lvalues are copied. Setters validate string rvalues before consuming them, and
section/key arguments may refer into that string. Allocation failures during an
insertion can leave an explicitly moved argument in a valid but unspecified state;
the manager itself retains its previous data.

Operations that can fail return `ini::result<T>` (`std::expected<T, ini::error>`)
and are `[[nodiscard]]`. The diagnostic owns its context and contains:

- `reason` and `op`: machine-readable enums;
- optional one-based `line` and byte `column`;
- `section_name`, `key_name`, optional `path`;
- `system_code` when a native operation provides one; generic `io_error` for
  streams without a reliable specific code;
- `replacement`, `cleanup_code`, and `temporary_path` for save failures.

`message()` renders a readable diagnostic, but callers should branch on enums,
not message text. Missing section/key, invalid format and out-of-range values
are distinct. String values are returned as `std::string`.

Diagnostic context is quoted and escaped for display, including native paths,
control characters and system messages. Structured fields retain their original
bytes. A leftover temporary file's path is included in the message when available.

`get_value_or_default` substitutes **only for absence**. A present empty string
is not absent. A malformed typed value remains an error. String literals,
`std::string` and `std::string_view` defaults all return owning strings. Character
arrays preserve their known length; a needed null C-string default is an error.

`expected` does not promise absence of exceptions. Allocation failures and
exceptions from user extraction/formatting propagate; allocating operations are
not `noexcept`. Caller stream I/O exceptions become expected I/O errors, while
`bad_alloc` propagates. Formatting and conversion occur before mutation.

## Strict INI dialect

Names are case-sensitive, with no Unicode normalization. Bytes above ASCII are
preserved without validating UTF-8. Only a UTF-8 BOM at the very beginning of the
input is removed. LF, CRLF, and a final line without a newline are supported;
a bare CR is invalid.

- Blank lines and lines whose first non-space/tab byte is `;` or `#` are ignored.
- A section is `[name]`; surrounding spaces/tabs inside brackets are syntax
  whitespace. After `]`, only spaces/tabs or a comment separated by them are
  allowed: `[new] ; comment` is valid; `[new];comment` is invalid.
- A pair is `key=value`. Surrounding spaces/tabs are removed from parsed names
  and values. Keys must be nonempty. Empty values are valid.
- Keys before the first header belong to section `""`. `[]` explicitly selects
  the same section. Empty documents contain no sections. Empty sections persist.
- Repeated section headers merge their keys. Duplicate keys anywhere in the same
  input document fail by default, even across repeated headers. Merge into an
  existing manager intentionally replaces existing values.
- Missing separators, malformed headers, embedded NUL, DEL and disallowed ASCII
  controls are errors, including inside comments. Internal tabs are permitted in
  values, but not in names.
- No permissive mode, continuation/multiline syntax, quoting or escaping exists.
  Quote and backslash characters in values are literal bytes.

Options are explicit:

```cpp
ini::parse_options options;
options.allow_colon = true;
options.inline_comments = true;
options.duplicates = ini::duplicate_policy::last_wins;
std::istringstream input("[s]\nkey: value ; explanation\n");
auto parsed = ini::ini_manager::from_stream(input, options);
```

With `allow_colon`, the first `=` or `:` is the separator. With inline comments,
`;` or `#` starts a comment only at the start of the value field or immediately
after a space/tab. Thus `a;b#c` and `https://host/#fragment` remain intact;
`value ; comment` becomes `value`. Inline comments are **off by default**, so
`;` and `#` are ordinarily literal everywhere in a value.

Options belong to the manager and can be inspected using `options()`. Successful
load replaces options (omitted options mean defaults); merge uses existing options.
Construct `ini_manager(options)` to use a selected dialect for programmatic data.

## Resource limits and canonical writing

| Option | Default | Meaning |
| --- | ---: | --- |
| `max_line_bytes` | 65,536 | Raw bytes excluding LF/CRLF, including any initial BOM |
| `max_input_bytes` | 16,777,216 | All input bytes, including BOM and line endings |
| `max_sections` | 4,096 | Unique sections, including a present global section |
| `max_keys` | 100,000 | Unique keys across all sections |

Zero is a literal zero limit, not unlimited. Checks occur before extending the
line buffer, not after an unlimited `getline`. File read-ahead is a fixed 4096
bytes. Allocation is bounded by configured input/count limits plus map overhead;
this is not a process-wide memory quota. Setters check counts; serialization
checks canonical line/document sizes, so a successfully parsed compact input can
still be too large to write in canonical spelling under the same limits.

The writer emits sorted sections/keys with ` = ` separators, LF endings, and an
explicit `[]` for a present global section. It does not preserve comments,
whitespace, ordering or original numeric spelling after a typed setter.

Mutators reject names/values that would change meaning on rereading:

- Names cannot have outer spaces/tabs or control bytes. Sections cannot contain
  brackets; keys cannot contain an active separator or start with `[`, `;`, `#`.
- Values cannot have outer spaces/tabs, NUL, newlines or disallowed controls.
- Under inline comments, values containing a comment-start sequence are rejected.

Nothing is silently trimmed by setters or the writer. For example, `" padded "`,
`"a\nb"`, and key `"a=b"` are rejected. Serialize validates all data and the full
canonical size before any output or target file access. Any successfully written
configuration rereads to equivalent data with the same options.

## Typed values

Integral values use decimal `from_chars`, require full consumption, and reject
leading `+`, hex, negative unsigned and trailing junk. Signed/unsigned char,
including `int8_t`/`uint8_t` aliases, are numbers; plain `char` requires one byte.
Overflow produces `out_of_range`. Unsupported types fail constraints at compile time.

Floating-point values use decimal/scientific charconv; leading `+`, hex, NaN and
infinity are rejected. Overflow and underflow reported by charconv are range
errors. Representable subnormal values and negative zero round-trip. Typed setters
use shortest round-tripping `to_chars` output, independent of global locale.

Bool accepts ASCII-case-insensitive `true`/`false` and `1`/`0`; `yes`/`no` and
`on`/`off` are not accepted. Arithmetic reading trims surrounding space/tab and
requires no other leftover bytes. String reads do not perform conversion.

Custom types must be default-initializable, movable and extractable using an
`istream& operator>>(istream&, T&)`. Extraction uses the classic locale and must
succeed, leaving only spaces/tabs or EOF. EOF alone is not proof of success.
Custom setters accept `std::formattable` types and validate the formatted result.
Custom extraction uses a classic-locale input stream over borrowed, read-only
configuration bytes. Extractors must not modify that buffer or retain references
to the stream or its storage; returned custom values must own any data they need.
Move-only extractable types are supported, including as defaults passed by value.
The library cannot guarantee a custom formatter/extractor pair round-trips.

Concurrent const operations on a manager are permitted if user conversions and
their dependencies are also safe for concurrent use. Synchronize mutation,
assignment, moving and destruction against all access to the same manager.
Callbacks must not mutate or destroy that manager reentrantly during a call;
extraction borrows its stored bytes until the callback returns. Independent
manager copies have independent storage. Synchronization of shared streams and
other callback state remains the caller's responsibility.

## Streams and file preservation

An input stream with preexisting failbit/badbit fails. Normal EOF sets only
eofbit, so `if (input >> config)` succeeds after a complete document. Stream
operators replace data, and honor exception masks: an eofbit exception can occur
*after a successful load has committed*. Expected stream methods absorb I/O
exceptions without changing masks or clearing existing errors. They report
syntax diagnostics through the result; `operator>>` additionally sets failbit.
A streambuf reporting EOF is treated as EOF; a custom source must signal actual
read failures with an exception rather than silently returning EOF.
Byte/count limits do not impose a timeout: reading a pipe, device or stalled
stream can block. File loads follow symlinks; the regular-file and symlink
restrictions below apply to save destinations.

`write_stream` checks both writing and `pubsync` (flush); it never closes a caller's
stream. Stream operators use the same mechanisms. Stream locale and formatting
flags do not affect canonical output.

`write_file` validates and serializes first, rejects directories/devices/symlinks,
creates a temporary file exclusively in the same directory, checks complete
writes and close, then replaces the target without first deleting it. Colliding
temporary names are retried a bounded number of times; existing files/symlinks
are never opened for truncation. Temporary files are cleaned up on failure;
cleanup errors accompany the original error without replacing it.

On POSIX the replacement is rename; on Windows it is `MoveFileExW` with
`MOVEFILE_REPLACE_EXISTING` and without copy fallback. The target directory must
be trusted and stable. Exclusive creation protects against preexisting symlinks,
but this is not a defense against an attacker who can rename parent directories
or unlink open temporary files. Concurrent saves have last-replacement-wins
semantics, without locking or conflict detection.

Failures before replacement preserve the old file. Replacement failures with an
uncertain outcome explicitly report `replacement_state::unknown`; callers must
not assume the old file is intact. Ordinary local POSIX rename provides atomic
visibility; filesystem/provider guarantees apply on Windows and network filesystems.
No power-loss durability is promised: close/flush is not fsync, and the directory
is not synchronized. No durable mode is exposed.

POSIX new files use mode 0600 (subject to umask); replacement preserves the
existing ordinary rwx bits, not special mode bits. Windows new files inherit
security from the directory. Ownership, ACLs, extended attributes, hardlink
identity and original formatting are not preserved by contract. Replacement
requires directory permissions and may fail on Windows if another process holds
the destination without delete sharing.
