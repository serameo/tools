# Phase 3: String Values

## Objective

Add lexer/grammar support for string values in both forms, `s(TEXT)`
and `"TEXT"`, with backslash escaping and multi-line support, and
extend the document model and public API to store and retrieve
strings.

## Tasks

1. `src/lexer.l`:
   - Rename the existing quoted-string token from `KEY` to `QSTRING`
     (it is used both as a key, before `=`, and as a string value,
     after `=` - the same lexeme, different grammar position).
   - Add a new rule for `s(TEXT)`, token `SSTRING`, matching
     `s\(([^)\\]|\\.)*\)`.
   - Add a shared `xconf_lexer_unescape()` helper applying backslash
     escaping to both forms: `\"` -> `"`, `\\` -> `\`, `\n` -> newline,
     `\t` -> tab, `\)` -> `)` (only meaningful inside `s(...)`, but
     harmless if written inside `"..."` too). An unrecognized escape
     (e.g. `\z`) is kept as-is (backslash + the character), rather
     than treated as an error - noted as an assumption.
   - Multi-line values fall out naturally: the negated character
     classes used (`[^"\\]`, `[^)\\]`) already match embedded newlines
     in flex, so no separate continuation rule is needed.
2. `src/parser.y`:
   - `string_value : QSTRING | SSTRING ;`
   - `entry : QSTRING '=' number_value ... | QSTRING '=' string_value {
     xconf_document_set_string(...); } ;`
3. `src/xconf_value.h` / `.c`:
   - Add `XCONF_TYPE_STRING` handling: the value union gets a `char
     *string` member (owned by the document).
   - Add `xconf_document_set_string()` (duplicates the string; frees
     any previously owned string when overwriting an entry, including
     when overwriting a number-typed entry that later becomes a
     string, or vice versa).
   - `xconf_document_destroy()` now also frees owned string values.
4. `include/xconf.h` / `src/xconf.c`:
   - Add `XCONF_TYPE_STRING` to `xconf_type_t`.
   - Add `xconf_get_string(xconf_t *cfg, const char *key, const char
     **out)`. The returned pointer is owned by the document and stays
     valid until the document is freed or the key's value is
     overwritten.
5. Tests:
   - Extend `tests/test_value_store.c` with string-specific checks
     (set/get a string, overwrite a number with a string and back,
     case-insensitive lookup still works).
   - Add `tests/test_phase3.c`: parses both string forms, an escaped
     quote/backslash/newline/tab, a `)` escaped inside `s(...)`, and a
     value that spans multiple physical lines.

## Files Touched

- src/lexer.l, src/parser.y (extended)
- src/xconf_value.h, src/xconf_value.c (extended)
- include/xconf.h, src/xconf.c (extended)
- tests/test_value_store.c (extended), tests/test_phase3.c (new)
- tests/CMakeLists.txt (add test_phase3)

## Exit / Test Criteria

- `ctest --test-dir build` passes `test_basic`, `test_value_store`,
  `test_phase2`, and `test_phase3` on a machine with flex, bison, and
  cmake installed.
- `test_phase3` specifically confirms: `s(...)` and `"..."` values
  round-trip correctly, `\"`, `\\`, `\n`, `\t`, and `\)` all decode as
  intended, and a string value spanning multiple lines in the source
  file is captured as a single value containing the embedded newline.

## Changes Log

- 2026-09-26: same sandbox limitation as Phases 1-2 - no network
  access, so flex/bison/cmake remain unavailable here. What WAS
  verified in this sandbox: the extended `xconf_value.c` (including
  the new string handling and the overwrite-frees-old-value logic)
  was compiled and exercised directly with a plain-gcc test, bypassing
  the lexer/parser. `xconf.c` was compiled alone to check for
  syntax/type errors against the updated headers. What was NOT
  verified here: that `lexer.l` (with the renamed `QSTRING` token and
  new `SSTRING` rule) and `parser.y` (with the new `string_value`
  grammar) are valid flex/bison input, and that `test_basic`,
  `test_phase2`, and `test_phase3` still pass end to end after the
  rename and additions. Action needed: rebuild with
  `cmake -B build && cmake --build build && ctest --test-dir build`
  and confirm before Phase 4 (arrays) begins.
