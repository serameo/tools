# XConf Library - Master Development Plan

## 1. Overview

- Project: a C library ("xconf") that reads and writes text configuration
  files in a key = value format.
- Target platforms: Windows and Linux (must compile cleanly on both).
- Language: C (C99/C11), portable, no platform-specific API calls in the
  core library.
- Parser tooling: flex (lexer) + bison (parser). Chosen over the original
  lex/yacc because flex/bison are actively maintained and available on
  both platforms (native packages on Linux, winflexbison on Windows via
  choco/vcpkg/MSYS2).
- Build system: CMake, so the same build description works with
  MSVC, MinGW, GCC, and Clang.
- Documentation, source code, and comments: ASCII English only.

## 2. Goals

- Correctly parse the key=value file format described in section 3.
- Provide a small, stable C API to: initialize, parse, query, add keys
  programmatically, and save/serialize back to the text format.
- Build and pass tests on both Windows and Linux.

## 3. Non-Goals (for version 1)

- No nested arrays or nested objects (only a flat array of scalars).
- No boolean value type (not present in the given format).
- No include/import directives between files.
- No multithreading guarantees (single-threaded use is assumed for v1).

## 4. File Format Specification (formalized from the request)

4.1 Comments
  - Any text after `#` or `!` up to end of line is a comment, EXCEPT
    when `#` or `!` appears inside a quoted/string value.

4.2 Key
  - A key is always a double-quoted string, e.g. "tax".
  - CONFIRMED: keys are case-insensitive. "Tax" and "tax" refer to the
    same key.

4.3 Assignment
  - Form: KEY = VALUE, optional whitespace allowed around `=`.

4.4 Number value
  - Two equivalent forms: `n(NUMBER)` and a bare `NUMBER`.
  - NUMBER = optional leading '-', digits, optional '.' + digits,
    optional exponent [eE][+-]?digits.
  - Examples: "tax"=n(0.07), "pi"=3.14e7, "x"=n(-1.23E-5)
  - CONFIRMED: internal storage is `double` only; no separate integer
    type is exposed.

4.5 String value
  - Two forms: `s(TEXT)` where TEXT runs until the matching `)`, and
    `"TEXT"` where TEXT runs until the matching, unescaped `"`.
  - Example: "appname"=s(hello world), "say_hello"="kon ni ji wa"
  - CONFIRMED: both forms support backslash escaping, C-string style:
    `\"` (quote), `\\` (backslash), `\n` (newline), `\t` (tab).
    Inside `s(...)`, `\)` is the escape for a literal `)` character
    that would otherwise close the value (same mechanism as `"..."`).

4.6 Array value
  - Two forms:
    - `a(elem1, elem2, ...)` where each element is itself `n(...)` or
      `s(...)`.
    - `[elem1, elem2, ...]` where each element is a bare number or a
      double-quoted string.
  - Example: "key"=[123, -1.23e6, "hello"]
  - Arrays may mix numbers and strings (heterogeneous).
  - CONFIRMED: arrays cannot contain arrays (no nesting).

4.7 Multi-line values
  - String and array values may continue onto the next physical line.
  - Working interpretation: the lexer simply keeps consuming characters
    (including newlines) until it reaches the value's closing delimiter
    (`)`, matching `"`, or `]`); there is no special continuation marker.

4.8 Whitespace and blank lines
  - Freely allowed between tokens and are otherwise ignored.

4.9 Duplicate keys
  - CONFIRMED: if a key appears more than once in a file, the later
    occurrence overwrites the value of the earlier one.

4.10 Encoding
  - CONFIRMED: files are pure ASCII only (this applies to file content
    parsed by the library, in addition to the library's own source
    code and documentation).

4.11 Hexadecimal / binary number literals (CONFIRMED - Phase 9, see
     xconf_phase9.md)
  - Two new number forms in addition to the ones in 4.4:
    `h(HEX)` for hexadecimal, e.g. "hex_number" = h(0x1234abcd), and
    `b(BINARY)` for binary, e.g. "bin_number" = b(11010011).
  - CONFIRMED: `h(...)` accepts all three of `0x`/`0X`-prefixed and
    bare (no prefix) hex digits, case-insensitive. `b(...)` is binary
    digits only (0/1), no prefix. Both accept an optional leading `-`
    for a negative value. Both are usable anywhere a number is today,
    including as `a(...)`/`[...]` array elements. The value is stored
    as the same `double` type as any other number, and on `xconf_save`
    is written back out in decimal, per Phase 5/7's existing
    "canonical form, not original spelling" rule - not re-rendered as
    `h(...)`/`b(...)`.
  - CONFIRMED (a refinement beyond the original decimal-number save
    rule): if a number's original literal WAS `h(...)` or `b(...)`, a
    comment recording that original text is added when the entry is
    saved, e.g. `# original: h(0x1234abcd)`, combined with any
    existing trailing comment on that line. This only applies to a
    top-level entry's own value, not to an individual element inside
    an array - see xconf_phase9.md's Scoping Note.

## 5. Public API (draft, subject to refinement during Phase 5-7)

```
typedef struct xconf xconf_t;

typedef enum {
    XCONF_TYPE_NONE = 0,
    XCONF_TYPE_NUMBER,
    XCONF_TYPE_STRING,
    XCONF_TYPE_ARRAY
} xconf_type_t;

xconf_t *xconf_init(void);
void xconf_free(xconf_t *cfg);

int xconf_parse_file(xconf_t *cfg, const char *path);
int xconf_parse_string(xconf_t *cfg, const char *text, size_t len);

xconf_type_t xconf_get_value_type(xconf_t *cfg, const char *key);
int xconf_get_number(xconf_t *cfg, const char *key, double *out);
int xconf_get_string(xconf_t *cfg, const char *key, const char **out);

int xconf_get_array_length(xconf_t *cfg, const char *key, size_t *out);
xconf_type_t xconf_get_element_type(xconf_t *cfg, const char *key, size_t index);
int xconf_get_number_element(xconf_t *cfg, const char *key, size_t index, double *out);
int xconf_get_string_element(xconf_t *cfg, const char *key, size_t index, const char **out);

int xconf_add_number(xconf_t *cfg, const char *key, double value);
int xconf_add_string(xconf_t *cfg, const char *key, const char *value);
int xconf_add_array_begin(xconf_t *cfg, const char *key);
int xconf_array_push_number(xconf_t *cfg, const char *key, double value);
int xconf_array_push_string(xconf_t *cfg, const char *key, const char *value);

int xconf_save(xconf_t *cfg, const char *path);
```

## 6. Architecture

- `lexer.l` (flex): tokenizes keys, numbers, string forms, punctuation
  (`= ( ) [ ] ,`), and skips whitespace, but does NOT discard comments
  (see the document-model note below).
- `parser.y` (bison): parses a sequence of key=value statements (plus
  comment-only and blank lines) into an in-memory representation.
- `xconf_value.c/h`: the value representation (tagged union) used by
  both the parser and the public API, independent of flex/bison types.
- `xconf.c/h`: the public API (init/parse/query/add), wrapping the core
  storage and the generated parser.
- `xconf_writer.c`: serializes the in-memory structure back to text for
  `xconf_save`.

CONFIRMED design note: `xconf_save` must preserve the original file's
layout and comments as closely as possible, not just regenerate a
clean canonical form. This means the internal representation cannot be
a plain key -> value hash map; it must retain the file as an ordered
sequence of "entries" (key=value lines, comment-only lines, and blank
lines), each remembering enough of its original text (comment content,
surrounding whitespace/line breaks for multi-line values) to be
written back unchanged when untouched. Updating an existing key (see
4.9, overwrite-on-duplicate) replaces only that entry's value in place
and keeps its position and any attached comment; keys added via
`xconf_add_*` are appended as new entries, typically at the end of the
document. This is a more involved data structure than a simple lookup
table and will be designed in detail during Phase 5.

## 7. Directory Structure (proposed)

```
xconf/
  CMakeLists.txt
  include/xconf.h
  src/
    xconf.c
    xconf_value.c
    xconf_value.h
    xconf_writer.c
    lexer.l
    parser.y
  tests/
    CMakeLists.txt
    test_basic.c
    fixtures/*.conf
  docs/
    xcomf_plans.md
    xconf_phase1.md
    xconf_phase2.md
    ...
  README.md
```

## 8. Build System Plan

- CMake >= 3.15.
- `find_package(FLEX REQUIRED)`, `find_package(BISON REQUIRED)`.
- `FLEX_TARGET` / `BISON_TARGET` / `ADD_FLEX_BISON_DEPENDENCY` to
  generate lexer.c / parser.c at build time.
- Static library target `xconf` (shared library optional later).
- `XCONF_BUILD_TESTS` option (default ON) wiring tests via CTest.
- README documents flex/bison installation on Linux (apt/dnf) and
  Windows (winflexbison via choco or vcpkg, or MSYS2).

## 9. Phase Breakdown

Each phase gets its own file (xconf_phaseN.md) with: objective, detailed
tasks, files touched, exit/test criteria, and a "Changes Log" section
that is appended to if the plan changes while working on that phase.
Development does not move to the next phase until the current phase's
tests are confirmed.

- Phase 1: Project scaffolding + build system (empty lexer/parser wired
  through CMake; project builds on Linux and Windows).
- Phase 2: Lexer + grammar for comments, whitespace, keys, and
  bare/n() numbers.
- Phase 3: Lexer + grammar for string values (s(...) and "...",
  including multi-line).
- Phase 4: Grammar for arrays (a(...) and [...] forms, mixed types).
- Phase 5: In-memory document model (ordered entries: key=value,
  comments, blank lines) + query API, with case-insensitive key
  lookup and overwrite-on-duplicate-key behavior.
- Phase 6: Programmatic add-key API (xconf_add_*), appending new
  entries to the document model.
- Phase 7: Save/serialize API (xconf_save) that reproduces the
  original layout and comments for untouched entries, with
  round-trip tests (parse -> save -> byte-compare for an unmodified
  file; parse -> modify one key -> save -> verify only that entry
  changed).
- Phase 8: Cross-platform validation (Windows MSVC + MinGW, Linux
  GCC/Clang), packaging, and documentation polish.

## 10. Confirmed Decisions (resolved 2026-09-26)

All format questions raised during planning have been answered and are
now reflected in section 4 and section 6 above:

1. Both `"..."` and `s(...)` strings support backslash escaping
   (`\"`, `\\`, `\n`, `\t`; `\)` inside `s(...)`).
2. (covered by 1) `s(...)` uses the same backslash-escape mechanism to
   allow a literal `)` inside the string.
3. Duplicate keys: the later occurrence overwrites the earlier one.
4. Keys are case-insensitive.
5. Arrays cannot be nested.
6. Files are pure ASCII only.
7. Numbers are stored/exposed as `double` only, no separate integer
   type.
8. `xconf_save` must preserve the original layout and comments as
   closely as possible (see the document-model design note in
   section 6), not just emit a clean canonical form.

No open questions remain. Phase 1 (build scaffolding) can proceed, and
Phase 2 onward can proceed using the decisions above without further
assumptions.

## 11. Progress Log

- Phase 1 (project scaffolding): code written; user confirmed the
  full `cmake --build` + `ctest` pipeline compiles and passes. DONE.
- Phase 2 (comments/whitespace/keys/numbers): code written, a link
  bug (missing xconf_value.c in the library sources) was found and
  fixed, and the user confirmed `ctest` passes. DONE.
- Phase 3 (string values `s(...)` and `"..."`, escaping, multi-line):
  code written, and the user confirmed `ctest` passes. DONE.
- Phase 4 (arrays `a(...)` and `[...]`, mixed number/string elements):
  code written, a bison `%union`/header bug (needed `%code requires`)
  was found and fixed, and the user confirmed `ctest` passes. DONE.
- Phase 5 (format-preserving document model + query API): the flat
  {key, value} store was replaced with an ordered list of nodes
  (entry / comment / blank), so comments and blank lines survive
  parsing for Phase 7's `xconf_save`. Duplicate keys are no longer
  merged in place: both occurrences stay in the document, and
  `xconf_document_find()` now returns the LAST match, so the public
  query API's observable behavior (Phases 2-4's tests) is unchanged.
  Lexer/grammar became line-oriented. User confirmed the build had no
  bison warnings or errors and `ctest` passed, and approved the
  scoping decisions (layout preserved at line/comment level, not
  byte-exact; duplicate keys both kept with last-wins lookup). DONE.
- Phase 6 (programmatic add API): added `xconf_add_number`,
  `xconf_add_string`, `xconf_add_array_begin`, `xconf_array_push_number`,
  `xconf_array_push_string` with insert-or-update-in-place semantics
  (deliberately different from parsing's append-only duplicate
  handling - see xconf_phase6.md). Found and fixed a naming collision
  between this phase's public `xconf_array_push_number/push_string`
  and Phase 4's identically-named internal array helpers (renamed the
  internal ones to `xconf_array_append_number/append_string`).
  `test_phase6.c` never calls the parser, so it was fully compiled AND
  RUN in the sandbox (not just link-checked) and passed. User
  confirmed build and ctest passed. DONE.
- Phase 7 (save/serialize): added `src/xconf_writer.c/.h`
  (`xconf_write_document()`) and the public `xconf_save()`. Canonical
  rendering chosen and documented in `xconf_phase7.md`: bare numbers
  (shortest of `%.15g`/`%.17g` that round-trips exactly), double-quoted
  escaped strings, bracket arrays; comments and blank lines reproduced
  verbatim; a value's original spelling (e.g. `n(...)` vs bare) is not
  preserved, per Phase 5's scoping decision. `tests/test_writer.c`
  builds a document via the internal add_* functions (no parser) and
  was fully compiled AND RUN in the sandbox, checking the exact
  rendered output - passed. `tests/test_phase7.c` (the real parse ->
  save -> reload round trip) could only be compiled here, since
  reloading needs real flex/bison. This is the last phase from the
  original plan. User confirmed the build succeeded and all tests
  passed. DONE.

## 12. Project Status: all planned phases complete

Phases 1 through 7 are all implemented and confirmed working
(`ctest` passing) on the user's machine. The library now supports:
parsing (comments, numbers, strings with escaping and multi-line
support, arrays, case-insensitive keys, duplicate-key handling),
querying every value type, programmatic construction via
`xconf_add_*`, and saving back to disk with comments/blank lines
preserved and values rendered canonically (see xconf_phase7.md).
Any further work (e.g. stricter error reporting/recovery, the
reentrant-parser and parse-error-cleanup limitations noted in
earlier phase docs, or a different canonical save style) would be a
new phase beyond the original plan and should get its own
xconf_phaseN.md if pursued.

## 13. Phase 9: hexadecimal / binary number literals

The user requested `h(0x1234abcd)` and `b(11010011)` number forms in
addition to the existing decimal ones (see section 4.11 above and
xconf_phase9.md). All open questions were answered and folded into
section 4.11 above. Implementation is complete: `lexer.l`/`parser.y`
gained a shared `BASENUM` token for both forms, and `xconf_value.c/h`
gained `xconf_compose_origin_comment()` so a hex/binary entry's
original literal survives as a comment even though the value itself is
always saved in decimal (per the existing canonical-save rule). Value-
store-level tests passed in the sandbox; `test_phase9.c` (the real
parse-level test) could only be compiled here, same as every other
phase's end-to-end test - pending the user's `ctest` confirmation.

## 14. Phase 10: xconf_foreach (iterating keys and values)

The user requested a way to visit every key/value pair in a document:
`xconf_foreach(int (*callback)(const char *key, xconf_value_t *value,
void *userdata), void *userdata)`. See xconf_phase10.md for the full
design - in short: `xconf_value_t` became a new PUBLIC type (a
read-only snapshot of one entry's value, including a read-only array
view), which required renaming the pre-existing INTERNAL struct of the
same name to `xconf_node_value_t` throughout `xconf_value.h`/`.c` and
`xconf_writer.c` (the latter had silently started compiling against
the wrong type the moment the public one was added - caught and fixed
before packaging). Duplicate keys are visited once, using their
current (last-wins) value, consistent with every other query function.
`test_phase10.c` passed compilation; the real parse-level run is
pending the user's `ctest` confirmation, as with every phase that
touches the parser.

## 15. Phase 8 (circled back to): cross-platform validation, packaging,
    documentation polish

Section 9's original Phase 8 was finally tackled directly, after
Phases 9 and 10. See xconf_phase8.md for full detail. Honest framing:
every build confirmation so far in this project actually came from
Linux (gcc) via WSL, never a native Windows compiler - so this phase
could not itself produce a Windows "PASS", only (a) a portability
audit that fixes the single most common real flex+MSVC failure
(`<unistd.h>` not existing there - `src/lexer.l` now sets
`%option nounistd`/`%option never-interactive`), (b) `CMakeLists.txt`
packaging/polish (`install()` rules, per-compiler warning flags,
strict C99), and (c) a GitHub Actions CI workflow
(`.github/workflows/ci.yml`) covering Linux GCC/Clang and Windows
MSVC/MinGW - the first mechanism this project has had for an actual
automated Windows build, though it is itself untested (this sandbox
has no network or Windows to run it on) and needs the user to push it
somewhere that runs it, or adapt it into a manual checklist.

**Update**: the user then confirmed, on a real Windows machine (Visual
Studio 2022 Build Tools, `winflexbison3` via Chocolatey) - not WSL -
that `cmake -B winbuild` (after install, with CMake auto-locating
`win_flex`/`win_bison` from `PATH`), `cmake --build winbuild --config
Debug`, and `ctest --test-dir winbuild -C Debug` all pass completely.
This is the project's first genuine native-Windows (MSVC) validation.
README.md's "Building on Windows" and "Platform support" sections now
present this as the confirmed path. MinGW is still unconfirmed on a
real machine; the GitHub Actions workflow is still unexercised.
