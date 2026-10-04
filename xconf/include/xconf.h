/*
 * xconf.h - public API for the xconf configuration file library.
 *
 * Phase 2: adds parsing (numbers only, via xconf_parse_string() and
 * xconf_parse_file()) and the corresponding query functions. String
 * and array value types, and the add/save API, arrive in later
 * phases.
 */

#ifndef XCONF_H
#define XCONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Opaque handle to a parsed/constructed configuration document. */
typedef struct xconf xconf_t;

typedef enum {
    XCONF_TYPE_NONE = 0,
    XCONF_TYPE_NUMBER,
    XCONF_TYPE_STRING,
    XCONF_TYPE_ARRAY
} xconf_type_t;

/*
 * xconf_array_element_t - one array element (Phase 10, used by
 * xconf_value_t.as.array below). type is always XCONF_TYPE_NUMBER or
 * XCONF_TYPE_STRING - arrays cannot contain arrays. string is owned
 * by the document; do not free or modify it, and do not retain it
 * beyond the xconf_foreach() callback invocation it was handed in
 * (see xconf_foreach() further down).
 */
typedef struct {
    xconf_type_t type;
    union {
        double number;
        char *string;
    } as;
} xconf_array_element_t;

/*
 * xconf_value_t - a read-only snapshot of one entry's value, passed
 * to the xconf_foreach() callback (see further down). Do not retain
 * this pointer, or its .as.string / .as.array.items, beyond that one
 * callback invocation - the data is only guaranteed valid for its
 * duration.
 */
typedef struct {
    xconf_type_t type;
    union {
        double number;  /* XCONF_TYPE_NUMBER */
        char *string;   /* XCONF_TYPE_STRING; owned by the document */
        struct {
            size_t count;
            const xconf_array_element_t *items; /* NULL if count == 0 */
        } array;         /* XCONF_TYPE_ARRAY */
    } as;
} xconf_value_t;

/*
 * xconf_init - create a new, empty xconf document.
 * Returns a pointer to the new document, or NULL on allocation failure.
 * The caller must release it with xconf_free().
 */
xconf_t *xconf_init(void);

/*
 * xconf_free - release all resources owned by a document created with
 * xconf_init(). Passing NULL is allowed and does nothing.
 */
void xconf_free(xconf_t *cfg);

/*
 * xconf_parse_string - parse xconf-format text from memory into cfg.
 * text does not need to be NUL-terminated; len gives its length in
 * bytes. Returns 0 on success, -1 on error (invalid arguments or a
 * parse error).
 */
int xconf_parse_string(xconf_t *cfg, const char *text, size_t len);

/*
 * xconf_parse_file - read and parse the xconf-format file at path
 * into cfg. Returns 0 on success, -1 if the file cannot be read or a
 * parse error occurs.
 */
int xconf_parse_file(xconf_t *cfg, const char *path);

/*
 * xconf_get_value_type - the type of the value stored under key, or
 * XCONF_TYPE_NONE if the key is not present. Key lookup is
 * case-insensitive.
 */
xconf_type_t xconf_get_value_type(xconf_t *cfg, const char *key);

/*
 * xconf_get_number - fetch the number stored under key into *out.
 * Returns 0 on success, -1 if the key is not present or is not a
 * number.
 */
int xconf_get_number(xconf_t *cfg, const char *key, double *out);

/*
 * xconf_get_string - fetch the string stored under key into *out.
 * The returned pointer is owned by cfg and stays valid until cfg is
 * freed or the key's value is overwritten; the caller must not free
 * it. Returns 0 on success, -1 if the key is not present or is not a
 * string.
 */
int xconf_get_string(xconf_t *cfg, const char *key, const char **out);

/*
 * xconf_get_array_length - fetch the number of elements in the array
 * stored under key into *out. Returns 0 on success, -1 if the key is
 * not present or is not an array.
 */
int xconf_get_array_length(xconf_t *cfg, const char *key, size_t *out);

/*
 * xconf_get_element_type - the type (XCONF_TYPE_NUMBER or
 * XCONF_TYPE_STRING) of the element at index within the array stored
 * under key, or XCONF_TYPE_NONE if key is not present, is not an
 * array, or index is out of range.
 */
xconf_type_t xconf_get_element_type(xconf_t *cfg, const char *key, size_t index);

/*
 * xconf_get_number_element - fetch the number at index within the
 * array stored under key into *out. Returns 0 on success, -1 if key
 * is not present, is not an array, index is out of range, or that
 * element is not a number.
 */
int xconf_get_number_element(xconf_t *cfg, const char *key, size_t index, double *out);

/*
 * xconf_get_string_element - fetch the string at index within the
 * array stored under key into *out (owned by cfg; see
 * xconf_get_string() for ownership/lifetime). Returns 0 on success,
 * -1 if key is not present, is not an array, index is out of range,
 * or that element is not a string.
 */
int xconf_get_string_element(xconf_t *cfg, const char *key, size_t index, const char **out);

/*
 * xconf_add_number - set key's value to a number: updates the
 * existing entry in place if key is already present (keeping its
 * position and any existing trailing comment), or appends a new entry
 * at the end of the document otherwise. This is deliberately
 * different from what happens while parsing a file, where a duplicate
 * key always appends a new line so nothing from the source is lost;
 * see xconf_phase5.md/xconf_phase6.md for why. Returns 0 on success,
 * -1 on allocation failure or invalid arguments.
 */
int xconf_add_number(xconf_t *cfg, const char *key, double value);

/*
 * xconf_add_string - like xconf_add_number(), but for a string value.
 * value is copied; the caller retains ownership of it.
 */
int xconf_add_string(xconf_t *cfg, const char *key, const char *value);

/*
 * xconf_add_array_begin - set key's value to a new, empty array
 * (same insert-or-update-in-place semantics as xconf_add_number()).
 * Follow with xconf_array_push_number()/xconf_array_push_string() to
 * fill it in. Returns 0 on success, -1 on allocation failure or
 * invalid arguments.
 */
int xconf_add_array_begin(xconf_t *cfg, const char *key);

/*
 * xconf_array_push_number - append a number to the array under key
 * (created with xconf_add_array_begin(), or already an array from
 * parsing). Returns 0 on success, -1 if key is not present or is not
 * an array, or on allocation failure.
 */
int xconf_array_push_number(xconf_t *cfg, const char *key, double value);

/*
 * xconf_array_push_string - like xconf_array_push_number(), but
 * appends a string (copied; the caller retains ownership of value).
 */
int xconf_array_push_string(xconf_t *cfg, const char *key, const char *value);

/*
 * xconf_save - write cfg to the file at path in xconf format. Each
 * entry keeps its original key and same-line trailing comment;
 * comment-only and blank lines are reproduced exactly. Values are
 * written in one canonical form (bare numbers, double-quoted strings,
 * bracket arrays) rather than preserving their original spelling -
 * see xconf_phase7.md for the exact rendering rules. Returns 0 on
 * success, -1 if the file cannot be written.
 */
int xconf_save(xconf_t *cfg, const char *path);

/*
 * xconf_foreach_cb - callback type for xconf_foreach() (below).
 * Called once per key currently in the document, in document order.
 * value is only valid for the duration of this one call - do not
 * retain the pointer, or its .as.string / .as.array.items, past it.
 * Returning a non-zero value stops iteration early; xconf_foreach()
 * then returns that same value to its caller. Returning 0 continues
 * iteration to the next key.
 */
typedef int (*xconf_foreach_cb)(const char *key, xconf_value_t *value, void *userdata);

/*
 * xconf_foreach - call callback once for every key currently in cfg,
 * in document order, with that key's current value (if a key was
 * written more than once, per Phase 5's "later duplicate overwrites
 * the earlier one for queries" rule, only its final/effective value
 * is visited - the superseded, earlier occurrence is skipped).
 * Comment-only and blank lines are not visited, since they have no
 * key/value.
 *
 * If callback returns non-zero, iteration stops immediately and
 * xconf_foreach() returns that value without visiting any further
 * keys. If every call returns 0, xconf_foreach() returns 0 once every
 * key has been visited. Returns -1 without calling callback at all if
 * cfg or callback is NULL.
 */
int xconf_foreach(xconf_t *cfg, xconf_foreach_cb callback, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* XCONF_H */
