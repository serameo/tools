# Phase 5: Format-Preserving Document Model + Full Query API

## Objective

Replace the flat {key, value} store from Phases 2-4 with an ordered
list of document *nodes* - entries, comment lines, and blank lines -
so that the structure needed by `xconf_save` (Phase 7) exists, and
finalize the query API's duplicate-key semantics on top of it.

## Scoping Decision (please review)

"Preserve the original layout and comments as closely as possible"
(confirmed decision 8) is implemented at the *line* level, not the
byte level:

- Comment lines and blank lines are preserved exactly, in their
  original order, position, and text (including which of `#`/`!` was
  used).
- Each entry keeps its original key text and its parsed value, plus
  an optional same-line trailing comment.
- What is NOT preserved: the exact original spelling/formatting of a
  value (e.g. whether a number was written as `n(0.07)` or bare
  `0.07`, or exactly how an array's elements were spaced across
  lines). Phase 7's writer will re-render each entry's value in one
  consistent canonical form.
- Comments that appear *inside* an array or `n(...)`/`a(...)`
  construct (between its parentheses/brackets) are dropped, not
  preserved. Only comments at the top level (their own line, or
  trailing after a complete value) are kept. This is called out
  because it is a real loss of fidelity for that edge case.

If this split (structural layout preserved, value spelling not) is
not what you had in mind, let me know before Phase 7 builds the
writer on top of it.

## Duplicate-Key Handling (revised)

To keep every physical line available for Phase 7 while still
honoring "a later duplicate key overwrites the earlier one" for
queries: **both** entries stay in the document, in their original
order, and `xconf_document_find()` now returns the *last* matching
entry instead of the first. Queries therefore see the later value,
exactly as confirmed, while nothing from the source file is discarded.

## Tasks

1. `src/lexer.l`:
   - Track a paren/bracket nesting depth (incremented on `(`/`[`,
     decremented on `)`/`]`).
   - `\n` is only returned as a NEWLINE token when depth is 0;
     otherwise it is treated as ordinary whitespace (so multi-line
     arrays keep working).
   - `#...`/`!...` are only returned as a COMMENT token (verbatim,
     including the marker) when depth is 0; otherwise they are
     dropped (see the scoping decision above).
2. `src/parser.y`: restructured around a line-oriented grammar:
   - `line : NEWLINE | COMMENT line_end | QSTRING '=' <value> trailing_comment line_end ;`
   - `line_end : NEWLINE | /* empty, at EOF */ ;`
   - `trailing_comment : /* empty */ | COMMENT ;`
3. `src/xconf_value.h` / `.c`: replaced the entry array with a node
   list (`xconf_node_t`: kind = ENTRY/COMMENT/BLANK). Renamed the
   parser-facing setters to `xconf_document_add_number/add_string/
   add_array/add_comment/add_blank` to reflect that they now always
   append (never overwrite in place); all of them take ownership of
   any pointer arguments regardless of success or failure, so callers
   never need to free them.
4. `src/xconf_internal.h`: added `xconf_lexer_depth` (reset before
   each parse) and `xconf_internal_document()` (test-only accessor to
   the opaque `xconf_t`'s internal document, so tests can inspect node
   structure without it being part of the public API).
5. `include/xconf.h` / `src/xconf.c`: `xconf_get_value_type/
   xconf_get_number/xconf_get_string/xconf_get_array_*` are unchanged
   at the API level; internally they now look up nodes instead of flat
   entries.
6. Tests:
   - Rewrote `tests/test_value_store.c` for the new node-based API
     (add_number/add_string/add_array/add_comment/add_blank,
     find-returns-last, destroy releasing everything).
   - Added `tests/test_phase5.c`: parses a mix of comments, blank
     lines, and a duplicate key; inspects the internal node list via
     `xconf_internal_document()` to confirm ordering/count/kinds, and
     confirms the public query API resolves the duplicate key to the
     later value.

## Files Touched

- src/lexer.l, src/parser.y (restructured)
- src/xconf_value.h, src/xconf_value.c (restructured)
- src/xconf_internal.h (extended)
- src/xconf.c (internal lookups updated; xconf.h unchanged)
- tests/test_value_store.c (rewritten), tests/test_phase5.c (new)
- tests/CMakeLists.txt (add test_phase5, give it access to src/)

## Exit / Test Criteria

- `ctest --test-dir build` passes `test_basic`, `test_value_store`,
  `test_phase2`, `test_phase3`, `test_phase4`, and `test_phase5`.
- `test_phase2`/`test_phase3`/`test_phase4` passing unchanged confirms
  the public API's observable behavior did not regress.
- `test_phase5` confirms comments/blank lines/duplicate keys are
  captured as described above.
- Please also check the `cmake --build` output for any bison
  "shift/reduce" or "reduce/reduce" conflict warnings this time (not
  just errors) and paste them if present - this phase's grammar is
  larger than before and could not be bison-verified in the sandbox.

## Changes Log

- 2026-09-26: same sandbox limitation as Phases 1-4 - no network
  access, so flex/bison/cmake remain unavailable here, and this is the
  largest single change to lexer.l/parser.y so far (a genuinely
  line-oriented grammar plus a depth counter for newline/comment
  suppression inside arrays). What WAS verified in this sandbox: the
  rewritten `xconf_value.c` was compiled and exercised directly with a
  rewritten `test_value_store.c`, covering add_number/add_string/
  add_array/add_comment/add_blank, find-returns-last-match, and
  correct release of strings/arrays on document destroy. `xconf.c` was
  compiled alone, and linked together with `xconf_value.c` and
  `test_basic.c` using stub replacements for the not-yet-generated
  flex/bison output, to catch missing-symbol issues before rebuilding.
  What was NOT verified here: that the restructured `lexer.l`/
  `parser.y` are valid flex/bison input, that the grammar is free of
  unwanted shift/reduce conflicts, and that `test_phase2` through
  `test_phase5` pass end to end. Given the size of this change, please
  paste the full `cmake --build` output (not just ctest results) so
  any bison warnings can be reviewed even if the build succeeds.
