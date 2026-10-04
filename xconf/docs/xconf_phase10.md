# Phase 10: xconf_foreach (Iterating Keys and Values)

## Objective

Add `xconf_foreach()`, letting a caller visit every key/value pair in
a document without knowing the keys in advance - requested as:

```c
xconf_foreach(int (*callback)(const char *key, xconf_value_t *value, void *userdata), void *userdata)
```

## Decisions Made While Implementing (not asked, but worth reviewing)

1. **`cfg` parameter added.** The function clearly needs to know
   which document to iterate, so the final signature is
   `int xconf_foreach(xconf_t *cfg, xconf_foreach_cb callback, void *userdata);`
   - the callback type itself matches the request exactly.
2. **`xconf_value_t` is now a *public* type**, defined in `xconf.h`,
   exposing a *read-only snapshot* of one entry's value: a number, a
   string, or (new) a read-only array view (`count` + a pointer to
   `xconf_array_element_t` elements, each a number or a string). This
   is the only way the callback can see an array's contents directly,
   since the callback signature (as requested) has no way to call back
   into `xconf_get_array_*` itself (those need `cfg`, which the
   callback doesn't receive). The data behind `value->as.string` and
   `value->as.array.items` is owned by the document and only valid for
   that one callback invocation - documented in `xconf.h`, not to be
   retained.
3. **Naming collision, found and fixed.** The library already had an
   *internal* struct called `xconf_value_t` (in `xconf_value.h`,
   holding a node's owned value - number/string/array *pointer*). That
   name is now taken by the new *public* type (different shape: the
   array case is an embedded read-only view, not an owning pointer).
   Renamed the internal one to `xconf_node_value_t` throughout
   `xconf_value.h`/`.c`. This also surfaced a second, separate
   instance of the same class of bug in `xconf_writer.c`, which had
   silently started compiling against the *wrong* (new, public)
   `xconf_value_t` the moment it was added to `xconf.h` - same root
   cause as the Phase 4 and Phase 6 naming collisions already on
   record, just one layer less obvious since it was a silent type
   rebind rather than a redeclaration error. Fixed by using
   `xconf_node_value_t` there too. Also renamed the internal
   `xconf_scalar_t` (one array element) to reuse the new public
   `xconf_array_element_t` directly, rather than keeping two
   identically-shaped but separately-named struct types.
4. **Duplicate keys: only the winning value is visited.** Per Phase
   5, a key written more than once keeps both occurrences in the
   document (for `xconf_save`), but `xconf_document_find()` resolves
   to the *last* one for queries. `xconf_foreach()` follows the same
   rule: for a key with more than one entry, only its current
   (last-wins) occurrence is visited, in that occurrence's position in
   the document; the superseded, earlier occurrence is skipped
   entirely rather than being visited with its stale value. This
   matches how every other query function already behaves - flagging
   it here since it was a real behavioral choice, not an accident, in
   case the alternative (visit every physical line, duplicates
   included) was actually wanted.
5. **Comment-only and blank lines are skipped** (they have no
   key/value to report).
6. **Stop-early semantics.** If `callback` returns non-zero,
   `xconf_foreach()` stops immediately and returns that *same* value
   (not just a generic `-1`), so the caller can tell both that it
   stopped early and what the callback reported. Returns `0` once
   every key has been visited without an early stop, and `-1` (without
   calling `callback` at all) if `cfg` or `callback` is NULL.

## Implementation

- `include/xconf.h`: added `xconf_array_element_t`, the public
  `xconf_value_t`, `xconf_foreach_cb`, and `xconf_foreach()`.
- `src/xconf_value.h`/`.c`: renamed the internal value struct to
  `xconf_node_value_t`; renamed `xconf_scalar_t` away in favor of the
  (now public) `xconf_array_element_t`, used directly by
  `xconf_array_t`.
- `src/xconf_writer.c`: updated to use `xconf_node_value_t` (see
  Decision 3).
- `src/parser.y`: its `%union`'s `scalarval` field now has type
  `xconf_array_element_t` (same rename, no behavioral change).
- `src/xconf.c`: `xconf_foreach()` walks `cfg->doc->nodes`, skipping
  non-entry nodes and superseded duplicate keys (via
  `xconf_document_find()`), building a public `xconf_value_t` per
  visited entry, and calling `callback`.

## Tests

- `tests/test_phase10.c` (new): full traversal (confirms comment/blank
  lines and the superseded duplicate `"tax"` are skipped, and the
  remaining three keys are visited in document order with correct
  types); invalid-argument handling; early stop (confirms the
  callback's own non-zero return value comes back out of
  `xconf_foreach()`); and reading an array's contents through the
  read-only view. This needs real parsing, so (like every other
  end-to-end test) it could only be compiled here, not run.
- No changes needed to `tests/test_value_store.c` - `xconf_foreach()`
  is tested at the public-API level only, since it's a thin layer over
  the existing node list and lookup function.

## Files Touched

- include/xconf.h (new public types + `xconf_foreach`)
- src/xconf_value.h, src/xconf_value.c (renames; see Decision 3)
- src/xconf_writer.c (rename fix)
- src/parser.y (rename, one field's type)
- src/xconf.c (`xconf_foreach` implementation)
- tests/test_phase10.c (new)
- tests/CMakeLists.txt (added test_phase10)

## Exit / Test Criteria

- `ctest --test-dir build` passes all tests, including the new
  `test_phase10`.
- `test_phase10` specifically confirms everything in the Decisions
  section above.

## Changes Log

- 2026-09-30: same sandbox limitation as every phase that touches the
  parser - no network access, so flex/bison/cmake remain unavailable
  here. What WAS verified: every source file (`xconf.h`,
  `xconf_value.h/.c`, `xconf_writer.c`, `xconf.c`) was recompiled after
  the rename and the new `xconf_foreach()`, catching the
  `xconf_writer.c` silent-rebind issue described in Decision 3 before
  it could reach the user's build; `test_value_store.c`, `test_writer.c`,
  and `test_phase6.c` (none of which touch the parser) were re-run in
  full and still pass; every test file, including the new
  `test_phase10.c`, compiles cleanly against the updated headers; a
  full link-check with a flex/bison stand-in stub also passed. What
  was NOT verified: an actual flex/bison build of the (lightly
  changed) `parser.y`, and `test_phase10.c`'s real run (it needs real
  parsing). Please rebuild and confirm `ctest` passes.
