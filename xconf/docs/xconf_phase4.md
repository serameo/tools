# Phase 4: Array Values

## Objective

Add lexer/grammar support for array values in both forms, `a(elem,
...)` (elements as `n(NUMBER)` or `s(STRING)`) and `[elem, ...]`
(elements as a bare NUMBER or a `"STRING"`), and extend the document
model and public API to store and query arrays. Per the confirmed
decisions, arrays may mix numbers and strings, but cannot contain
nested arrays.

## Tasks

1. `src/lexer.l`: add single-character tokens for `a`, `[`, `]`, `,`.
2. `src/parser.y`:
   - `array_value : 'a' '(' a_element_list_opt ')' | '[' bracket_element_list_opt ']' ;`
   - `a_element : 'n' '(' NUMBER ')' | SSTRING ;`
   - `bracket_element : NUMBER | QSTRING ;`
   - `a_element_list` / `bracket_element_list` accumulate elements
     left-to-right into an `xconf_array_t`, built incrementally as
     each element is reduced (ownership of any parsed string is
     transferred straight into the array, no extra copy).
   - `entry : ... | QSTRING '=' array_value { xconf_document_set_array(...); free($1); } ;`
3. `src/xconf_value.h` / `.c`:
   - Add `xconf_scalar_t` (a `{ type, { number | string } }` pair for
     one array element) and `xconf_array_t` (growable array of
     `xconf_scalar_t`).
   - Add `xconf_array_create/destroy`, `xconf_array_push_number`,
     `xconf_array_push_string` (takes ownership of the string).
   - Add `XCONF_TYPE_ARRAY` handling to the value union
     (`xconf_array_t *array`) and to `xconf_value_release()`
     (recursively frees the array's owned strings, then the array).
   - Add `xconf_document_set_array()` - takes ownership of the array
     pointer it is given regardless of whether it succeeds or fails
     (the caller never needs to clean it up itself).
4. `include/xconf.h` / `src/xconf.c`:
   - Add `XCONF_TYPE_ARRAY` to `xconf_type_t`.
   - Add `xconf_get_array_length()`, `xconf_get_element_type()`,
     `xconf_get_number_element()`, `xconf_get_string_element()`.
5. Tests:
   - Extend `tests/test_value_store.c` with array-specific checks
     (build an array directly via `xconf_array_*`, store it, read
     elements back, overwrite an array with a plain number and make
     sure the old array is released).
   - Add `tests/test_phase4.c`: parses both array forms, an empty
     array, a mixed number/string array, and confirms
     `xconf_get_array_length` / `xconf_get_element_type` /
     `xconf_get_number_element` / `xconf_get_string_element` all
     report correctly.

## Known Limitation

Parser-action cleanup on a syntax error is not implemented (no
bison `%destructor` yet): if parsing fails partway through an array,
the partially-built `xconf_array_t` for that array is leaked rather
than freed. This is consistent with Phases 2-3, which also do not
handle parse-error cleanup, and is left as a follow-up rather than
blocking this phase, since the confirmed scope has not asked for
strict error recovery yet.

## Files Touched

- src/lexer.l, src/parser.y (extended)
- src/xconf_value.h, src/xconf_value.c (extended)
- include/xconf.h, src/xconf.c (extended)
- tests/test_value_store.c (extended), tests/test_phase4.c (new)
- tests/CMakeLists.txt (add test_phase4)

## Exit / Test Criteria

- `ctest --test-dir build` passes `test_basic`, `test_value_store`,
  `test_phase2`, `test_phase3`, and `test_phase4` on a machine with
  flex, bison, and cmake installed.
- `test_phase4` specifically confirms: both array forms parse, an
  empty array works, a mixed number/string array preserves per-element
  types and values in order, and array length/element queries behave
  correctly for out-of-range indices and non-array keys.

## Changes Log

- 2026-09-26: same sandbox limitation as Phases 1-3 - no network
  access, so flex/bison/cmake remain unavailable here. What WAS
  verified in this sandbox: the extended `xconf_value.c` (array
  create/destroy/push, and set_array's overwrite/release behavior)
  was compiled and exercised directly with a plain-gcc test, bypassing
  the lexer/parser. `xconf.c` was compiled alone, and `xconf.c` +
  `xconf_value.c` + `test_basic.c` were linked together (with stub
  replacements for the not-yet-generated flex/bison output) to catch
  any missing-symbol issue before the user rebuilds. What was NOT
  verified here: that the extended `lexer.l`/`parser.y` (new `a`, `[`,
  `]`, `,` tokens and the array grammar) are valid flex/bison input,
  and that `test_phase4` (and the other tests) pass end to end.
  Action needed: rebuild with
  `cmake -B build && cmake --build build && ctest --test-dir build`
  and confirm before Phase 5 (the format-preserving document model and
  full query API) begins.

- 2026-09-26 (bug fix): the user's build reported a compile error this
  sandbox could not catch: `unknown type name 'xconf_array_t'` and
  `'xconf_scalar_t'` while compiling the generated `lexer.yy.c`, which
  includes `parser.tab.h`. Cause: `#include "xconf_value.h"` was only
  in `parser.y`'s `%{ ... %}` prologue, which bison emits into
  `parser.tab.c` only - not into the generated `parser.tab.h`, which is
  where the `%union` (referencing `xconf_array_t`/`xconf_scalar_t`) is
  actually written. Since `lexer.l` includes `parser.tab.h` directly,
  those types were undefined there. Fix: moved
  `#include "xconf_value.h"` into a `%code requires { ... }` block in
  `parser.y`, which bison does propagate into `parser.tab.h`. This is
  a well-known bison pattern (any type used in `%union` must be made
  available to the generated header via `%code requires`, not the
  plain prologue). Could not be re-verified by actually running
  flex/bison in this sandbox (still unavailable); the fix is a
  standard, narrowly-scoped one. Please rebuild again and confirm.
