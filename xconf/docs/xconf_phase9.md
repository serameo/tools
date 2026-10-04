# Phase 9: Hexadecimal / Binary Number Literals

## Objective

Add two new number forms, alongside the existing `n(NUMBER)`/bare
decimal from Phase 2:
- `h(HEX)` - hexadecimal, e.g. `"hex_number" = h(0x1234abcd)`
- `b(BINARY)` - binary, e.g. `"bin_number" = b(11010011)`

Both parse to the same `double`-valued number as any other number
value; there is still only one number type (`XCONF_TYPE_NUMBER`).

## Confirmed Decisions

1. `h(...)` accepts all three forms: `h(0x1234abcd)`, `h(0X1234ABCD)`,
   and bare `h(1234abcd)` with no prefix at all.
2. Both `h(...)` and `b(...)` accept an optional leading `-`, e.g.
   `h(-0x10)`, `b(-1010)`.
3. Hex digits are case-insensitive (`a`-`f`/`A`-`F` both accepted, in
   any mix).
4. Both forms are usable anywhere a number can appear today: as a
   top-level value, and as an element inside `a(...)` or `[...]`
   arrays.
5. On `xconf_save`, the value is written back out as an ordinary
   base-10 number, like every other number (Phase 5/7's "canonical
   form, not original spelling" rule still applies to the numeral
   itself) - **but** if the value's original literal was `h(...)` or
   `b(...)`, a comment recording that original form is added to the
   entry, e.g. `# original: h(0x1234abcd)`, combined with any existing
   trailing comment on that line (e.g.
   `# original: h(0x1234abcd) # my note`), or added as a new comment
   if there wasn't one.

## Scoping Note: origin comments only apply to top-level entries

The origin-preserving comment above is attached to a `"key" =
h(...)`/`b(...)` entry's own value. It is **not** attached to an
individual element inside an array (`a(h(0x10), ...)` or `[h(0x10),
...]`): arrays don't have a place for a per-element comment in the
current document model (only a whole entry line has a trailing-comment
slot - see Phase 5). Inside an array, a hex/binary element is simply
read as its numeric value with no comment added; only its top-level,
non-array use gets the origin note. Flagging this now in case it is
not what was expected - it's a deliberate scope limit, not an
oversight.

## Implementation

- `src/lexer.l`: two new rules matching the *entire* `h(...)`/`b(...)`
  construct as one token each (the same approach `s(...)` already uses
  for `SSTRING`) - not built from separate `'h'`/`'('`/`')'` characters
  like `n(...)` is, because the token needs to carry both the parsed
  value and the literal's original source text.
  - `h\(-?(0[xX])?[0-9a-fA-F]+\)` and `b\(-?[01]+\)` both return one
    shared token, `BASENUM`.
  - Conversion is done by a small hand-written function,
    `xconf_lexer_parse_based_number()` (handles the optional leading
    `-` and, for base 16, the optional `0x`/`0X` prefix, then
    accumulates digits into an `unsigned long long` before negating),
    rather than relying on `strtoll` - avoids any libc/locale edge
    cases and keeps the conversion fully under the project's control.
  - The token's value (`xconf_numlit_t { double value; char *origin;
    }`) carries the converted number plus a verbatim copy of the whole
    matched text (e.g. `"h(0x1234abcd)"`), which is what later becomes
    the save-time comment.
- `src/xconf_value.h` / `.c`: `xconf_compose_origin_comment(char
  *origin_literal, char *existing_comment)` takes ownership of both
  arguments and returns the comment to actually store - unchanged
  `existing_comment` if `origin_literal` is NULL; otherwise a new
  `"# original: <origin_literal>"` string, with `" <existing_comment>"`
  appended if there was one.
- `src/parser.y`:
  - `number_value`'s type is now `xconf_numlit_t` instead of a plain
    `double`: `NUMBER` and `'n' '(' NUMBER ')'` set `origin = NULL`;
    `BASENUM` passes its token value straight through.
  - The `line` rule's number-value alternative calls
    `xconf_compose_origin_comment()` to combine `number_value.origin`
    with the parsed `trailing_comment` before calling
    `xconf_document_add_number()`.
  - `a_element`/`bracket_element` gained a `BASENUM` alternative that
    uses only `.value` and frees `.origin` (see the Scoping Note).

## Tests

- Extended `tests/test_value_store.c` with direct unit tests of
  `xconf_compose_origin_comment()` (no existing comment, an existing
  comment, and `origin_literal == NULL` pass-through, including the
  double-NULL case) - fully runnable in the sandbox, since it never
  touches the parser.
- Added `tests/test_phase9.c`: parses `h(...)` in all three prefix
  forms (confirming they produce the same value), `b(...)`, negative
  versions of both, both forms inside `a(...)` and `[...]` arrays, and
  entries that combine (or don't have) an existing trailing comment
  with an origin note - checked via the public query API and via
  `xconf_internal_document()` for the exact resulting comment text
  (including confirming an array-valued entry gets NO origin comment,
  per the Scoping Note, even though some of its elements came from
  `h()`/`b()`).

## Files Touched

- src/lexer.l, src/parser.y (extended)
- src/xconf_value.h, src/xconf_value.c (new
  `xconf_numlit_t` type and `xconf_compose_origin_comment` helper)
- tests/test_value_store.c (extended), tests/test_phase9.c (new)
- tests/CMakeLists.txt (added test_phase9)
- docs/xcomf_plans.md section 4.11 (marked CONFIRMED)

## Changes Log

- 2026-09-28: same sandbox limitation as every phase that touches the
  parser - no network access, so flex/bison/cmake remain unavailable
  here. What WAS verified: the extended `xconf_value.c`
  (`xconf_compose_origin_comment`, all cases) was compiled and
  exercised directly via the extended `test_value_store.c` and passed.
  `xconf_value.c`/`xconf.c`/`xconf_writer.c` were compiled and
  link-checked as usual with stubs standing in for the not-yet-
  generated flex/bison output; `test_phase6.c` and `test_writer.c`
  (which don't touch the parser) were re-run in full and still pass.
  All test files, including the new `test_phase9.c`, compile cleanly
  against the updated headers. What was NOT verified: the new
  `lexer.l` rules (`BASENUM`'s regexes and hand-written base
  conversion) and the extended `parser.y` grammar -
  `tests/test_phase9.c` could only be compiled, not run, here since it
  needs real parsing. Please rebuild and confirm `ctest` passes.
