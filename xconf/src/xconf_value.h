/*
 * xconf_value.h - internal document/value model.
 *
 * Phase 5: the document is now an ORDERED LIST OF NODES rather than a
 * flat {key, value} map, so that comments and blank lines can be
 * preserved for xconf_save (Phase 7). A node is one of:
 *   - XCONF_NODE_ENTRY:   a "key" = value line, with an optional
 *                         same-line trailing comment.
 *   - XCONF_NODE_COMMENT: a comment-only line (verbatim text,
 *                         including its '#' or '!' marker).
 *   - XCONF_NODE_BLANK:   a blank line.
 *
 * Duplicate keys: BOTH occurrences are kept as separate ENTRY nodes,
 * in their original order (nothing from the source is discarded).
 * xconf_document_find() returns the LAST matching entry, so queries
 * see "a later duplicate key overwrites the earlier one" as
 * confirmed, while xconf_save() (Phase 7) can still reproduce every
 * original line.
 *
 * See xcomf_plans.md / xconf_phase5.md for the full scoping notes on
 * what "preserve the original layout" means here (structure and
 * comments are preserved; a value's original spelling/formatting is
 * not).
 */

#ifndef XCONF_VALUE_H
#define XCONF_VALUE_H

#include <stddef.h>
#include "xconf.h"

/*
 * Array elements are xconf_array_element_t (defined in xconf.h,
 * public, since Phase 10's xconf_foreach() exposes arrays directly to
 * callers - a number or a string, never another array).
 */
typedef struct {
    xconf_array_element_t *items;
    size_t count;
    size_t capacity;
} xconf_array_t;

xconf_array_t *xconf_array_create(void);
void xconf_array_destroy(xconf_array_t *arr);

/* Returns 0 on success, -1 on allocation failure. */
int xconf_array_append_number(xconf_array_t *arr, double value);

/*
 * xconf_array_append_string - append a string element. Takes
 * ownership of `value` (freed by xconf_array_destroy(); the caller
 * must not use or free it afterwards, whether this call succeeds or
 * fails).
 */
int xconf_array_append_string(xconf_array_t *arr, char *value);

/*
 * xconf_numlit_t - a parsed number value plus, for Phase 9's
 * hex/binary literals, the original source text. `origin` is NULL
 * for an ordinary decimal or n(...) number; for a value written as
 * h(...) or b(...), it is that literal's exact source text (e.g.
 * "h(0x1234abcd)"), owned by whoever holds this struct, so that
 * xconf_compose_origin_comment() (below) can preserve it as a comment
 * even though the value itself is always stored/written as decimal.
 */
typedef struct {
    double value;
    char *origin;
} xconf_numlit_t;

/*
 * xconf_node_value_t - the value held by an ENTRY node (a superset of
 * the public, read-only xconf_value_t in xconf.h used by
 * xconf_foreach(): this internal version owns its string/array
 * memory, where the public one is just a borrowed view). Named
 * distinctly from xconf.h's xconf_value_t to avoid a redeclaration
 * clash - the two are deliberately different types with a similar
 * shape.
 */
typedef struct {
    xconf_type_t type;
    union {
        double number;         /* XCONF_TYPE_NUMBER */
        char  *string;         /* XCONF_TYPE_STRING; owned by the node */
        xconf_array_t *array;  /* XCONF_TYPE_ARRAY; owned by the node */
    } as;
} xconf_node_value_t;

typedef enum {
    XCONF_NODE_ENTRY = 0,
    XCONF_NODE_COMMENT,
    XCONF_NODE_BLANK
} xconf_node_kind_t;

typedef struct {
    xconf_node_kind_t kind;

    /* Valid when kind == XCONF_NODE_ENTRY: */
    char *key;               /* original key text, case preserved; owned */
    xconf_node_value_t value;
    char *trailing_comment;  /* same-line comment, verbatim with its
                                 marker, or NULL; owned */

    /* Valid when kind == XCONF_NODE_COMMENT: */
    char *comment_text;      /* verbatim, with its marker; owned */
} xconf_node_t;

typedef struct xconf_document {
    xconf_node_t *nodes;
    size_t count;
    size_t capacity;
} xconf_document_t;

xconf_document_t *xconf_document_create(void);
void xconf_document_destroy(xconf_document_t *doc);

/*
 * xconf_document_find - case-insensitive key lookup among ENTRY
 * nodes, returning the index of the LAST match (later duplicate keys
 * override earlier ones for queries). Returns (size_t)-1 if key is
 * not present among any entry node.
 */
size_t xconf_document_find(const xconf_document_t *doc, const char *key);

/*
 * xconf_document_add_number/add_string/add_array - append a new
 * ENTRY node. Every pointer argument (key, value, trailing_comment)
 * is OWNED by the callee from the moment of the call: it will be
 * freed by the document eventually, or freed immediately on failure -
 * the caller must not use or free it afterwards either way.
 * Returns 0 on success, -1 on allocation failure or invalid arguments
 * (doc or key is NULL).
 */
int xconf_document_add_number(xconf_document_t *doc, char *key, double value, char *trailing_comment);
int xconf_document_add_string(xconf_document_t *doc, char *key, char *value, char *trailing_comment);
int xconf_document_add_array(xconf_document_t *doc, char *key, xconf_array_t *value, char *trailing_comment);

/*
 * xconf_document_add_comment - append a COMMENT node. Takes ownership
 * of comment_text as described above.
 */
int xconf_document_add_comment(xconf_document_t *doc, char *comment_text);

/*
 * xconf_document_add_blank - append a BLANK node.
 */
int xconf_document_add_blank(xconf_document_t *doc);

/*
 * xconf_document_upsert_number / xconf_document_upsert_string -
 * insert-or-update semantics for the programmatic add API (as opposed
 * to xconf_document_add_number()/add_string(), which the parser uses
 * and which always append, even for a duplicate key - see
 * xconf_phase5.md). key and value are copied; the caller retains
 * ownership of both. If key already names an entry (per
 * xconf_document_find(), so the LAST matching entry if there are
 * duplicates from parsing), that entry's value is replaced in place,
 * keeping its position and any existing trailing_comment; otherwise a
 * new entry is appended with no trailing comment. Returns 0 on
 * success, -1 on allocation failure or invalid arguments.
 */
int xconf_document_upsert_number(xconf_document_t *doc, const char *key, double value);
int xconf_document_upsert_string(xconf_document_t *doc, const char *key, const char *value);

/*
 * xconf_document_upsert_array_begin - insert-or-replace key's value
 * with a new, empty array (same in-place-if-present semantics as
 * xconf_document_upsert_number()). Follow with
 * xconf_document_array_push_number()/push_string() to fill it in.
 * Returns 0 on success, -1 on allocation failure or invalid
 * arguments.
 */
int xconf_document_upsert_array_begin(xconf_document_t *doc, const char *key);

/*
 * xconf_document_array_push_number / xconf_document_array_push_string
 * - find key (which must already hold an array, typically from
 * xconf_document_upsert_array_begin()) and append an element to it.
 * value is copied for push_string; the caller retains ownership.
 * Returns 0 on success, -1 if key is not present, does not hold an
 * array, or on allocation failure.
 */
int xconf_document_array_push_number(xconf_document_t *doc, const char *key, double value);
int xconf_document_array_push_string(xconf_document_t *doc, const char *key, const char *value);

/*
 * xconf_compose_origin_comment - build the trailing comment for an
 * entry whose number value came from a hex/binary literal (Phase 9),
 * so xconf_save() can preserve that fact even though the value itself
 * is always written back out in decimal.
 *
 * Takes ownership of BOTH arguments (each is freed internally as
 * needed) and returns a new, malloc'd comment string:
 *   - if origin_literal is NULL, existing_comment is returned
 *     unchanged (nothing to add - this is the ordinary decimal/n(...)
 *     case).
 *   - otherwise, a comment of the form "# original: <origin_literal>"
 *     is built, with " <existing_comment>" appended if
 *     existing_comment was not NULL (both survive, concatenated into
 *     one comment, since everything after the first '#'/'!' to end of
 *     line is comment text anyway).
 * Returns NULL if origin_literal was non-NULL but allocation failed
 * (the comment is silently dropped in that rare case; both inputs
 * have already been freed either way).
 */
char *xconf_compose_origin_comment(char *origin_literal, char *existing_comment);

#endif /* XCONF_VALUE_H */
