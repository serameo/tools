# Phase 6: Programmatic Add API

## Objective

Add the `xconf_add_*` / `xconf_array_push_*` functions from the
original API sketch, so a document can be built or modified in memory
(not just parsed from text) before being saved in Phase 7.

## Naming Collision Found and Fixed

Phase 4 already defined an *internal* helper
`xconf_array_push_number(xconf_array_t *arr, double value)` in
`xconf_value.h`, used by the parser to fill in an array while parsing.
The public API sketched back in `xcomf_plans.md` section 5 uses the
*same name* for a different signature:
`xconf_array_push_number(xconf_t *cfg, const char *key, double value)`.
Both would have been declared in `xconf.c` at once (it includes both
headers), which is a plain C redeclaration error. Resolved by renaming
the Phase 4 internal helpers to `xconf_array_append_number` /
`xconf_array_append_string` (updated in `xconf_value.h/.c`, `parser.y`,
and `tests/test_value_store.c`); the public names stay as originally
planned.

## Insert-or-Update Semantics (a deliberate difference from parsing)

While *parsing* a file, a duplicate key always appends a new node
(Phase 5), so every source line survives for `xconf_save`. The
programmatic add API is different on purpose: calling
`xconf_add_number()`/`xconf_add_string()`/`xconf_add_array_begin()`
with a key that already exists **updates that entry's value in place**
(keeping its position and any existing trailing comment) instead of
appending a duplicate. This matches ordinary "set a value" expectations
for code building a document, rather than the file-fidelity concern
that motivated Phase 5's append-only parsing behavior. If a key is not
already present, a new entry is appended at the end.

## Tasks

1. `src/xconf_value.h` / `.c`:
   - Rename the Phase 4 array-element helpers as described above.
   - Add `xconf_document_upsert_number`, `xconf_document_upsert_string`
     (both copy their key/value arguments; the caller retains
     ownership), and `xconf_document_upsert_array_begin` (replaces or
     creates the entry with a new, empty array).
   - Add `xconf_document_array_push_number` /
     `xconf_document_array_push_string` (look up key, which must
     already hold an array, and append to it; push_string copies its
     argument).
2. `include/xconf.h` / `src/xconf.c`: add the public functions
   `xconf_add_number`, `xconf_add_string`, `xconf_add_array_begin`,
   `xconf_array_push_number`, `xconf_array_push_string`, each a thin
   wrapper over the xconf_value.c functions above.
3. Tests:
   - Update `tests/test_value_store.c` for the renamed internal array
     helpers (no behavior change, just the new names).
   - Add `tests/test_phase6.c`: builds a document from scratch with
     `xconf_init` + `xconf_add_*` (no parsing involved), covering a
     number, a string, an array built via `add_array_begin` +
     `array_push_*`, and confirms that adding a key a second time
     updates it in place rather than creating a duplicate (checked via
     `xconf_internal_document()`'s node count, the same test-only
     accessor Phase 5 introduced).

## Files Touched

- src/xconf_value.h, src/xconf_value.c (renamed helpers, new
  upsert/push functions)
- src/parser.y (updated calls to the renamed helpers)
- include/xconf.h, src/xconf.c (new public functions)
- tests/test_value_store.c (renamed calls), tests/test_phase6.c (new)
- tests/CMakeLists.txt (add test_phase6)

## Exit / Test Criteria

- `ctest --test-dir build` passes all tests, including the new
  `test_phase6`.
- `test_phase6` specifically confirms: a document built purely via
  `xconf_add_*` (no `xconf_parse_*` call) round-trips correctly
  through the query API, and re-adding an existing key updates it in
  place (node count does not grow) while a genuinely new key does
  append a node.

## Changes Log

- 2026-09-27: same sandbox limitation as Phases 1-5 for anything that
  goes through the parser - no network access, so flex/bison/cmake
  remain unavailable here. This phase's `parser.y` change is small
  (only renaming two function calls), so the risk to the grammar
  itself is low compared to Phase 5.
  - `test_phase6.c` never calls `xconf_parse_*`, so - unlike every
    previous phase's end-to-end test - it could be compiled AND RUN
    for real in this sandbox: `xconf.c` + `xconf_value.c` +
    `test_phase6.c` were linked against a stub standing in for the
    not-yet-generated flex/bison output (the stub is never actually
    invoked, since nothing in this test parses) and it passed:
    `test_phase6: OK`.
  - The renamed/extended `xconf_value.c` was also re-verified via the
    updated `test_value_store.c`.
  - `xconf.c` was compiled on its own to confirm the naming-collision
    fix (public `xconf_array_push_number(xconf_t*, ...)` vs. the
    now-renamed internal `xconf_array_append_number(xconf_array_t*,
    ...)`) actually resolves the redeclaration error - it does.
  - What was NOT verified here: an actual flex/bison build of the
    (lightly changed) `parser.y`, and `test_basic`/`test_phase2`
    through `test_phase5` after this phase's other file changes.
  Please rebuild and confirm the full `ctest` suite passes before
  Phase 7 (save/serialize) begins.
