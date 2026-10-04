/*
 * xconf_value.c - see xconf_value.h for design notes.
 */

#include "xconf_value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * xconf_ascii_strcasecmp - portable ASCII case-insensitive compare.
 * Avoids relying on strcasecmp (POSIX) or _stricmp (MSVC-only), so
 * the same source builds cleanly on both Windows and Linux.
 */
static int xconf_ascii_strcasecmp(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/*
 * xconf_string_dup - portable strdup replacement (strdup itself is
 * POSIX, not standard C). Used by the upsert/push functions below,
 * which copy their arguments rather than taking ownership (unlike the
 * parser-facing xconf_document_add_* functions).
 */
static char *xconf_string_dup(const char *s)
{
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

/* ---- arrays ---------------------------------------------------- */

xconf_array_t *xconf_array_create(void)
{
    xconf_array_t *arr = (xconf_array_t *)malloc(sizeof(xconf_array_t));
    if (arr == NULL) {
        return NULL;
    }
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

void xconf_array_destroy(xconf_array_t *arr)
{
    size_t i;

    if (arr == NULL) {
        return;
    }
    for (i = 0; i < arr->count; i++) {
        if (arr->items[i].type == XCONF_TYPE_STRING) {
            free(arr->items[i].as.string);
        }
    }
    free(arr->items);
    free(arr);
}

static int xconf_array_reserve(xconf_array_t *arr, size_t needed)
{
    size_t new_capacity;
    xconf_array_element_t *new_items;

    if (arr->capacity >= needed) {
        return 0;
    }

    new_capacity = (arr->capacity == 0) ? 8 : arr->capacity * 2;
    if (new_capacity < needed) {
        new_capacity = needed;
    }

    new_items = (xconf_array_element_t *)realloc(arr->items, new_capacity * sizeof(xconf_array_element_t));
    if (new_items == NULL) {
        return -1;
    }

    arr->items = new_items;
    arr->capacity = new_capacity;
    return 0;
}

int xconf_array_append_number(xconf_array_t *arr, double value)
{
    if (arr == NULL) {
        return -1;
    }
    if (xconf_array_reserve(arr, arr->count + 1) != 0) {
        return -1;
    }
    arr->items[arr->count].type = XCONF_TYPE_NUMBER;
    arr->items[arr->count].as.number = value;
    arr->count++;
    return 0;
}

int xconf_array_append_string(xconf_array_t *arr, char *value)
{
    if (arr == NULL) {
        free(value);
        return -1;
    }
    if (xconf_array_reserve(arr, arr->count + 1) != 0) {
        free(value);
        return -1;
    }
    arr->items[arr->count].type = XCONF_TYPE_STRING;
    arr->items[arr->count].as.string = value; /* ownership transferred */
    arr->count++;
    return 0;
}

/* ---- value release ---------------------------------------------- */

static void xconf_value_release(xconf_node_value_t *value)
{
    if (value->type == XCONF_TYPE_STRING) {
        free(value->as.string);
    } else if (value->type == XCONF_TYPE_ARRAY) {
        xconf_array_destroy(value->as.array);
    }
    value->type = XCONF_TYPE_NONE;
}

/* ---- document / nodes -------------------------------------------- */

xconf_document_t *xconf_document_create(void)
{
    xconf_document_t *doc = (xconf_document_t *)malloc(sizeof(xconf_document_t));
    if (doc == NULL) {
        return NULL;
    }
    doc->nodes = NULL;
    doc->count = 0;
    doc->capacity = 0;
    return doc;
}

void xconf_document_destroy(xconf_document_t *doc)
{
    size_t i;

    if (doc == NULL) {
        return;
    }
    for (i = 0; i < doc->count; i++) {
        xconf_node_t *node = &doc->nodes[i];
        switch (node->kind) {
            case XCONF_NODE_ENTRY:
                free(node->key);
                free(node->trailing_comment);
                xconf_value_release(&node->value);
                break;
            case XCONF_NODE_COMMENT:
                free(node->comment_text);
                break;
            case XCONF_NODE_BLANK:
                break;
        }
    }
    free(doc->nodes);
    free(doc);
}

size_t xconf_document_find(const xconf_document_t *doc, const char *key)
{
    size_t i;
    size_t found = (size_t)-1;

    if (doc == NULL || key == NULL) {
        return (size_t)-1;
    }
    for (i = 0; i < doc->count; i++) {
        if (doc->nodes[i].kind == XCONF_NODE_ENTRY
            && xconf_ascii_strcasecmp(doc->nodes[i].key, key) == 0) {
            found = i; /* keep going: later matches win */
        }
    }
    return found;
}

static int xconf_document_reserve(xconf_document_t *doc, size_t needed)
{
    size_t new_capacity;
    xconf_node_t *new_nodes;

    if (doc->capacity >= needed) {
        return 0;
    }

    new_capacity = (doc->capacity == 0) ? 8 : doc->capacity * 2;
    if (new_capacity < needed) {
        new_capacity = needed;
    }

    new_nodes = (xconf_node_t *)realloc(doc->nodes, new_capacity * sizeof(xconf_node_t));
    if (new_nodes == NULL) {
        return -1;
    }

    doc->nodes = new_nodes;
    doc->capacity = new_capacity;
    return 0;
}

int xconf_document_add_number(xconf_document_t *doc, char *key, double value, char *trailing_comment)
{
    xconf_node_t *node;

    if (doc == NULL || key == NULL) {
        free(key);
        free(trailing_comment);
        return -1;
    }
    if (xconf_document_reserve(doc, doc->count + 1) != 0) {
        free(key);
        free(trailing_comment);
        return -1;
    }

    node = &doc->nodes[doc->count];
    node->kind = XCONF_NODE_ENTRY;
    node->key = key;
    node->value.type = XCONF_TYPE_NUMBER;
    node->value.as.number = value;
    node->trailing_comment = trailing_comment;
    node->comment_text = NULL;
    doc->count++;
    return 0;
}

int xconf_document_add_string(xconf_document_t *doc, char *key, char *value, char *trailing_comment)
{
    xconf_node_t *node;

    if (doc == NULL || key == NULL || value == NULL) {
        free(key);
        free(value);
        free(trailing_comment);
        return -1;
    }
    if (xconf_document_reserve(doc, doc->count + 1) != 0) {
        free(key);
        free(value);
        free(trailing_comment);
        return -1;
    }

    node = &doc->nodes[doc->count];
    node->kind = XCONF_NODE_ENTRY;
    node->key = key;
    node->value.type = XCONF_TYPE_STRING;
    node->value.as.string = value;
    node->trailing_comment = trailing_comment;
    node->comment_text = NULL;
    doc->count++;
    return 0;
}

int xconf_document_add_array(xconf_document_t *doc, char *key, xconf_array_t *value, char *trailing_comment)
{
    xconf_node_t *node;

    if (doc == NULL || key == NULL || value == NULL) {
        free(key);
        xconf_array_destroy(value);
        free(trailing_comment);
        return -1;
    }
    if (xconf_document_reserve(doc, doc->count + 1) != 0) {
        free(key);
        xconf_array_destroy(value);
        free(trailing_comment);
        return -1;
    }

    node = &doc->nodes[doc->count];
    node->kind = XCONF_NODE_ENTRY;
    node->key = key;
    node->value.type = XCONF_TYPE_ARRAY;
    node->value.as.array = value;
    node->trailing_comment = trailing_comment;
    node->comment_text = NULL;
    doc->count++;
    return 0;
}

int xconf_document_add_comment(xconf_document_t *doc, char *comment_text)
{
    xconf_node_t *node;

    if (doc == NULL || comment_text == NULL) {
        free(comment_text);
        return -1;
    }
    if (xconf_document_reserve(doc, doc->count + 1) != 0) {
        free(comment_text);
        return -1;
    }

    node = &doc->nodes[doc->count];
    node->kind = XCONF_NODE_COMMENT;
    node->comment_text = comment_text;
    node->key = NULL;
    node->trailing_comment = NULL;
    node->value.type = XCONF_TYPE_NONE;
    doc->count++;
    return 0;
}

int xconf_document_add_blank(xconf_document_t *doc)
{
    xconf_node_t *node;

    if (doc == NULL) {
        return -1;
    }
    if (xconf_document_reserve(doc, doc->count + 1) != 0) {
        return -1;
    }

    node = &doc->nodes[doc->count];
    node->kind = XCONF_NODE_BLANK;
    node->key = NULL;
    node->trailing_comment = NULL;
    node->comment_text = NULL;
    node->value.type = XCONF_TYPE_NONE;
    doc->count++;
    return 0;
}

/* ---- programmatic add API (insert-or-update) --------------------- */

int xconf_document_upsert_number(xconf_document_t *doc, const char *key, double value)
{
    size_t idx;
    char *key_copy;

    if (doc == NULL || key == NULL) {
        return -1;
    }

    idx = xconf_document_find(doc, key);
    if (idx != (size_t)-1) {
        xconf_value_release(&doc->nodes[idx].value);
        doc->nodes[idx].value.type = XCONF_TYPE_NUMBER;
        doc->nodes[idx].value.as.number = value;
        return 0;
    }

    key_copy = xconf_string_dup(key);
    if (key_copy == NULL) {
        return -1;
    }
    /* xconf_document_add_number takes ownership of key_copy. */
    return xconf_document_add_number(doc, key_copy, value, NULL);
}

int xconf_document_upsert_string(xconf_document_t *doc, const char *key, const char *value)
{
    size_t idx;
    char *key_copy;
    char *value_copy;

    if (doc == NULL || key == NULL || value == NULL) {
        return -1;
    }

    idx = xconf_document_find(doc, key);
    if (idx != (size_t)-1) {
        value_copy = xconf_string_dup(value);
        if (value_copy == NULL) {
            return -1;
        }
        xconf_value_release(&doc->nodes[idx].value);
        doc->nodes[idx].value.type = XCONF_TYPE_STRING;
        doc->nodes[idx].value.as.string = value_copy;
        return 0;
    }

    key_copy = xconf_string_dup(key);
    if (key_copy == NULL) {
        return -1;
    }
    value_copy = xconf_string_dup(value);
    if (value_copy == NULL) {
        free(key_copy);
        return -1;
    }
    /* xconf_document_add_string takes ownership of key_copy/value_copy. */
    return xconf_document_add_string(doc, key_copy, value_copy, NULL);
}

int xconf_document_upsert_array_begin(xconf_document_t *doc, const char *key)
{
    size_t idx;
    xconf_array_t *arr;
    char *key_copy;

    if (doc == NULL || key == NULL) {
        return -1;
    }

    arr = xconf_array_create();
    if (arr == NULL) {
        return -1;
    }

    idx = xconf_document_find(doc, key);
    if (idx != (size_t)-1) {
        xconf_value_release(&doc->nodes[idx].value);
        doc->nodes[idx].value.type = XCONF_TYPE_ARRAY;
        doc->nodes[idx].value.as.array = arr;
        return 0;
    }

    key_copy = xconf_string_dup(key);
    if (key_copy == NULL) {
        xconf_array_destroy(arr);
        return -1;
    }
    /* xconf_document_add_array takes ownership of key_copy/arr. */
    return xconf_document_add_array(doc, key_copy, arr, NULL);
}

int xconf_document_array_push_number(xconf_document_t *doc, const char *key, double value)
{
    size_t idx;

    if (doc == NULL || key == NULL) {
        return -1;
    }
    idx = xconf_document_find(doc, key);
    if (idx == (size_t)-1 || doc->nodes[idx].value.type != XCONF_TYPE_ARRAY) {
        return -1;
    }
    return xconf_array_append_number(doc->nodes[idx].value.as.array, value);
}

int xconf_document_array_push_string(xconf_document_t *doc, const char *key, const char *value)
{
    size_t idx;
    char *value_copy;

    if (doc == NULL || key == NULL || value == NULL) {
        return -1;
    }
    idx = xconf_document_find(doc, key);
    if (idx == (size_t)-1 || doc->nodes[idx].value.type != XCONF_TYPE_ARRAY) {
        return -1;
    }
    value_copy = xconf_string_dup(value);
    if (value_copy == NULL) {
        return -1;
    }
    /* xconf_array_append_string takes ownership of value_copy. */
    return xconf_array_append_string(doc->nodes[idx].value.as.array, value_copy);
}

/* ---- Phase 9: hex/binary literal origin comments ------------------ */

char *xconf_compose_origin_comment(char *origin_literal, char *existing_comment)
{
    static const char prefix[] = "# original: ";
    char *result;
    size_t len;

    if (origin_literal == NULL) {
        return existing_comment; /* nothing to add */
    }

    if (existing_comment != NULL) {
        /* "# original: <origin_literal> <existing_comment>" */
        len = strlen(prefix) + strlen(origin_literal) + 1 + strlen(existing_comment) + 1;
        result = (char *)malloc(len);
        if (result != NULL) {
            snprintf(result, len, "%s%s %s", prefix, origin_literal, existing_comment);
        }
        free(existing_comment);
    } else {
        /* "# original: <origin_literal>" */
        len = strlen(prefix) + strlen(origin_literal) + 1;
        result = (char *)malloc(len);
        if (result != NULL) {
            snprintf(result, len, "%s%s", prefix, origin_literal);
        }
    }

    free(origin_literal);
    return result;
}
