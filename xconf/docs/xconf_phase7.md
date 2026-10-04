# Phase 7: Save / Serialize

## Objective

Implement `xconf_save()`, writing a document's node list back out as
xconf-format text, per the scoping decision agreed in Phase 5:
comments and blank lines are reproduced exactly (position and text);
each entry keeps its original key and same-line trailing comment, but
its *value* is written in one consistent canonical form rather than
preserving the original spelling.

## Canonical Rendering Rules (new in this phase - please review)

Since a value's original spelling is not preserved (per Phase 5's
scoping decision), Phase 7 had to pick one canonical form to always
write. Chosen:

- Numbers: bare (never `n(...)`), formatted with `%.15g`, falling back
  to `%.17g` only if that does not round-trip back to the exact same
  `double` (a well-known technique to keep common values short - like
  `0.07` - while still guaranteeing any value survives a save/reload
  cycle exactly).
- Strings: always `"..."` (never `s(...)`), with `"`, `\`, newline,
  and tab escaped back to `\"`, `\\`, `\n`, `\t`.
- Arrays: always `[...]` (never `a(...)`), elements comma-and-space
  separated, each rendered per the rules above.
- Keys: always `"..."`, using the same string-escaping as above.
- A key/value line is written as `"key" = <value>`, followed by
  ` <trailing_comment>` if the entry has one, then a newline.
- Comment nodes are written verbatim (their stored text already
  includes the `#`/`!` marker) followed by a newline. Blank nodes are
  written as a bare newline. The file is written in binary mode so the
  output always uses a plain `\n` line ending regardless of platform.

If a different canonical style is wanted (e.g. `s(...)` instead of
`"..."`, or preserving `n(...)` for values that used it), this is the
place to change it - it's isolated in `src/xconf_writer.c`.

## Tasks

1. `src/xconf_writer.h` / `.c` (new): `xconf_write_document(FILE *fp,
   const xconf_document_t *doc)`, implementing the rules above.
   Returns 0 on success, -1 if a write error occurs (checked via
   `ferror(fp)`).
2. `include/xconf.h` / `src/xconf.c`: add `xconf_save(xconf_t *cfg,
   const char *path)`, opening `path` in binary write mode, calling
   `xconf_write_document()`, and checking both that call's result and
   `fclose()`'s result.
3. `CMakeLists.txt`: add `src/xconf_writer.c` to the `xconf` library's
   sources (Phase 2's exact bug - a new source file left out of
   `add_library()` - was watched for specifically here).
4. Tests:
   - `tests/test_writer.c` (new): builds a document directly via the
     internal `xconf_document_add_*` functions (no parser involved),
     writes it with `xconf_write_document()` to a `tmpfile()`, and
     checks the exact output text - fully runnable in the sandbox,
     since it never touches the lexer/parser.
   - `tests/test_phase7.c` (new): parses a sample file with comments,
     blank lines, and a trailing comment; saves it; reloads the saved
     file into a fresh `xconf_t`; and confirms the reloaded values
     match and the reloaded document's node structure (kinds, order,
     comment text) matches the original - the genuine round-trip test,
     but it needs real parsing for the *reload* step, so (like the
     other end-to-end tests) it could only be compiled, not run, here.

## Files Touched

- src/xconf_writer.h, src/xconf_writer.c (new)
- include/xconf.h, src/xconf.c (add xconf_save)
- CMakeLists.txt (add xconf_writer.c to the library)
- tests/test_writer.c (new), tests/test_phase7.c (new)
- tests/CMakeLists.txt (add both)

## Exit / Test Criteria

- `ctest --test-dir build` passes all tests, including the new
  `test_writer` and `test_phase7`.
- `test_phase7` specifically confirms a full parse -> save -> reload
  cycle preserves values, comments, and blank-line structure.

## Changes Log

- 2026-09-27: same sandbox limitation as every prior phase for
  anything that goes through the parser. `test_writer.c` never calls
  the parser (it builds its document via `xconf_document_add_*`
  directly and only exercises the new `xconf_write_document()`), so it
  was fully compiled AND RUN in this sandbox - checking the exact
  rendered text, including number formatting, string escaping, array
  formatting, and trailing-comment placement - and it passed.
  `xconf.c` (with the new `xconf_save()`) was compiled alone and
  linked with `xconf_value.c`, `xconf_writer.c`, and `test_basic.c`
  using a stub in place of the not-yet-generated flex/bison output, to
  catch missing-symbol issues (this is also where the CMakeLists.txt
  "did I remember to add the new .c file" check was done). What was
  NOT verified here: `test_phase7.c`'s actual reload step, which needs
  real flex/bison. Please rebuild and confirm the full `ctest` suite
  passes; this closes out the phases from the original plan.
