# Phase 2: Lexer/Grammar for Comments, Whitespace, Keys, and Numbers

## Objective

Extend the flex lexer and bison grammar to recognize: comments
(`#...` and `!...`), whitespace, double-quoted keys, and number
values in both forms (`n(NUMBER)` and a bare `NUMBER`). Parsed
key/number pairs are stored in a minimal internal document model
(case-insensitive key lookup, later-occurrence-overwrites-earlier per
the confirmed duplicate-key rule) so the behavior can be tested
end to end via `xconf_parse_string()`.

String and array value types are explicitly out of scope for this
phase (Phase 3 and Phase 4). The full format-preserving document
model (needed for `xconf_save`) is also out of scope; this phase's
storage is a plain array of {key, number} entries, replaced/extended
by the real document model in Phase 5.

## Tasks

1. `src/lexer.l`: add rules for
   - `#...` and `!...` comments (to end of line): ignored.
   - whitespace/newlines: ignored.
   - a double-quoted KEY token (text between the quotes, no escape
     processing yet - escaping is added in Phase 3 and reused here).
   - a NUMBER token matching `-?[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?`.
   - single-character tokens for `n`, `(`, `)`, `=` used by the
     `n(NUMBER)` form and the `KEY = value` assignment.
2. `src/parser.y`: grammar
   - `document : /* empty */ | document entry ;`
   - `entry : KEY '=' number_value { store it; free(KEY text); } ;`
   - `number_value : NUMBER | 'n' '(' NUMBER ')' ;`
3. `src/xconf_value.h` / `src/xconf_value.c`: minimal document model
   - `xconf_document_t`: growable array of `{ key, xconf_value_t }`.
   - `xconf_document_create` / `xconf_document_destroy`.
   - `xconf_document_find` (case-insensitive).
   - `xconf_document_set_number` (overwrite-if-present, else append).
4. `src/xconf_internal.h`: declares the global parse target pointer
   used by the (currently non-reentrant) parser actions to reach the
   document being built. Revisiting reentrancy is deferred; noted as
   a known limitation.
5. `src/lexer_bridge.h`: forward declarations for the flex-generated
   `yy_scan_string` / `YY_BUFFER_STATE` / `yyparse`, so `xconf.c` can
   drive the lexer/parser without needing flex's own scanner header.
6. `include/xconf.h`: add `xconf_type_t`, `xconf_parse_string`,
   `xconf_parse_file`, `xconf_get_value_type`, `xconf_get_number`.
7. `src/xconf.c`: implement the functions added to the header.
   `xconf_parse_file` reads the whole file into memory (binary mode)
   and delegates to `xconf_parse_string`.
8. Tests:
   - `tests/test_value_store.c`: exercises `xconf_value.c` directly
     (create/find/set/overwrite/case-insensitivity), independent of
     the lexer/parser, so this part can be verified even where
     flex/bison are unavailable.
   - `tests/test_phase2.c`: exercises `xconf_parse_string()` with
     comments, bare numbers, `n(...)` numbers, duplicate keys, and
     mixed-case key lookup.

## Files Touched

- src/lexer.l, src/parser.y (extended)
- src/xconf_value.h, src/xconf_value.c (new)
- src/xconf_internal.h (new)
- src/lexer_bridge.h (new)
- include/xconf.h, src/xconf.c (extended)
- tests/test_value_store.c, tests/test_phase2.c (new)
- tests/CMakeLists.txt (updated to add the new test executables)

## Exit / Test Criteria

- `ctest --test-dir build` passes `test_basic`, `test_value_store`,
  and `test_phase2` on a machine with flex, bison, and cmake
  installed (Linux and/or Windows).
- `test_phase2` specifically confirms: comments are ignored, both
  number forms parse to the same value, a later duplicate key
  overwrites an earlier one, and key lookup is case-insensitive.

## Changes Log

- 2026-09-26: Same sandbox limitation as Phase 1: no network access,
  so flex/bison/cmake are still not installed here, and the
  flex/bison-generated files (`lexer.yy.c`, `parser.tab.c/h`) could
  not be produced or compiled in this environment.
  - What WAS verified here: `src/xconf_value.c` was compiled and
    exercised directly with `tests/test_value_store.c` via plain
    `gcc -std=c99 -Wall -Wextra` (no flex/bison dependency) - passed,
    including case-insensitive lookup and overwrite-on-duplicate-key.
    `src/xconf.c` was also compiled alone (`gcc -c`) to confirm it has
    no syntax/type errors against `xconf.h`, `xconf_value.h`,
    `xconf_internal.h`, and `lexer_bridge.h` - compiled cleanly with
    no warnings. `tests/test_phase2.c` could NOT be run, since it
    needs the real generated lexer/parser linked into the `xconf`
    library.
  - What was NOT verified here: that `src/lexer.l` and `src/parser.y`
    are valid flex/bison input, that they generate code matching the
    `lexer_bridge.h` declarations used by `xconf.c`, and that
    `test_basic` and `test_phase2` pass end to end.
  - Action needed from the user: same as Phase 1 - build with
    `cmake -B build && cmake --build build && ctest --test-dir build`
    on a machine with flex, bison, and cmake, and report back whether
    all three tests (`test_basic`, `test_value_store`, `test_phase2`)
    pass, before Phase 3 begins.

- 2026-09-26 (bug fix): the user's build reported the exact error this
  sandbox could not catch on its own: linker errors for
  `xconf_document_create`, `xconf_document_destroy`,
  `xconf_document_find`, and `xconf_document_set_number` when linking
  `test_basic`. Cause: the top-level `CMakeLists.txt`'s `add_library(xconf ...)`
  call listed `src/xconf.c` but not `src/xconf_value.c`, so the
  document-store functions were never compiled into `libxconf.a`.
  Fix: added `src/xconf_value.c` to that source list. Re-verified in
  this sandbox by compiling `xconf.c` + `xconf_value.c` + `test_basic.c`
  together (with stand-in stubs for the not-yet-generated flex/bison
  output, since flex/bison are still unavailable here) - links and
  runs cleanly now. The real flex/bison-generated build still needs to
  be re-run by the user to confirm.
