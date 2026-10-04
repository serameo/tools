/*
 * xconf.c - public API implementation.
 *
 * Phase 5: the document is now xconf_value.c's ordered node list
 * (entries/comments/blanks). xconf_parse_string() drives the
 * flex/bison pipeline, resetting the shared parse-target and lexer
 * depth globals before each parse. xconf_parse_file() reads the whole
 * file into memory and delegates to xconf_parse_string().
 */

#include "xconf.h"
#include "xconf_value.h"
#include "xconf_internal.h"
#include "lexer_bridge.h"
#include "xconf_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Definitions of the globals declared in xconf_internal.h. */
xconf_document_t *xconf_current_parse_target = NULL;
int xconf_lexer_depth = 0;

struct xconf {
    xconf_document_t *doc;
};

xconf_document_t *xconf_internal_document(xconf_t *cfg)
{
    return (cfg != NULL) ? cfg->doc : NULL;
}

xconf_t *xconf_init(void)
{
    xconf_t *cfg = (xconf_t *)malloc(sizeof(xconf_t));
    if (cfg == NULL) {
        return NULL;
    }
    cfg->doc = xconf_document_create();
    if (cfg->doc == NULL) {
        free(cfg);
        return NULL;
    }
    return cfg;
}

void xconf_free(xconf_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    xconf_document_destroy(cfg->doc);
    free(cfg);
}

int xconf_parse_string(xconf_t *cfg, const char *text, size_t len)
{
    char *nul_terminated;
    YY_BUFFER_STATE buf;
    int rc;

    if (cfg == NULL || (text == NULL && len != 0)) {
        return -1;
    }

    /* yy_scan_string() requires a NUL-terminated string; make a copy
     * so callers are not required to NUL-terminate their buffer. */
    nul_terminated = (char *)malloc(len + 1);
    if (nul_terminated == NULL) {
        return -1;
    }
    if (len > 0) {
        memcpy(nul_terminated, text, len);
    }
    nul_terminated[len] = '\0';

    buf = yy_scan_string(nul_terminated);
    if (buf == NULL) {
        free(nul_terminated);
        return -1;
    }

    xconf_current_parse_target = cfg->doc;
    xconf_lexer_depth = 0;
    rc = yyparse();
    xconf_current_parse_target = NULL;

    yy_delete_buffer(buf);
    free(nul_terminated);

    return (rc == 0) ? 0 : -1;
}

int xconf_parse_file(xconf_t *cfg, const char *path)
{
    FILE *fp;
    long size;
    char *buffer;
    size_t read_count;
    int rc;

    if (cfg == NULL || path == NULL) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return -1;
    }

    read_count = fread(buffer, 1, (size_t)size, fp);
    fclose(fp);

    if (read_count != (size_t)size) {
        free(buffer);
        return -1;
    }
    buffer[size] = '\0';

    rc = xconf_parse_string(cfg, buffer, (size_t)size);
    free(buffer);
    return rc;
}

xconf_type_t xconf_get_value_type(xconf_t *cfg, const char *key)
{
    size_t idx;

    if (cfg == NULL || key == NULL) {
        return XCONF_TYPE_NONE;
    }
    idx = xconf_document_find(cfg->doc, key);
    if (idx == (size_t)-1) {
        return XCONF_TYPE_NONE;
    }
    return cfg->doc->nodes[idx].value.type;
}

int xconf_get_number(xconf_t *cfg, const char *key, double *out)
{
    size_t idx;

    if (cfg == NULL || key == NULL || out == NULL) {
        return -1;
    }
    idx = xconf_document_find(cfg->doc, key);
    if (idx == (size_t)-1) {
        return -1;
    }
    if (cfg->doc->nodes[idx].value.type != XCONF_TYPE_NUMBER) {
        return -1;
    }
    *out = cfg->doc->nodes[idx].value.as.number;
    return 0;
}

int xconf_get_string(xconf_t *cfg, const char *key, const char **out)
{
    size_t idx;

    if (cfg == NULL || key == NULL || out == NULL) {
        return -1;
    }
    idx = xconf_document_find(cfg->doc, key);
    if (idx == (size_t)-1) {
        return -1;
    }
    if (cfg->doc->nodes[idx].value.type != XCONF_TYPE_STRING) {
        return -1;
    }
    *out = cfg->doc->nodes[idx].value.as.string;
    return 0;
}

/*
 * xconf_find_array - shared helper: look up key and confirm it holds
 * an array. Returns the array pointer, or NULL if key is not present
 * or is not an array.
 */
static xconf_array_t *xconf_find_array(xconf_t *cfg, const char *key)
{
    size_t idx;

    if (cfg == NULL || key == NULL) {
        return NULL;
    }
    idx = xconf_document_find(cfg->doc, key);
    if (idx == (size_t)-1) {
        return NULL;
    }
    if (cfg->doc->nodes[idx].value.type != XCONF_TYPE_ARRAY) {
        return NULL;
    }
    return cfg->doc->nodes[idx].value.as.array;
}

int xconf_get_array_length(xconf_t *cfg, const char *key, size_t *out)
{
    xconf_array_t *arr;

    if (out == NULL) {
        return -1;
    }
    arr = xconf_find_array(cfg, key);
    if (arr == NULL) {
        return -1;
    }
    *out = arr->count;
    return 0;
}

xconf_type_t xconf_get_element_type(xconf_t *cfg, const char *key, size_t index)
{
    xconf_array_t *arr = xconf_find_array(cfg, key);

    if (arr == NULL || index >= arr->count) {
        return XCONF_TYPE_NONE;
    }
    return arr->items[index].type;
}

int xconf_get_number_element(xconf_t *cfg, const char *key, size_t index, double *out)
{
    xconf_array_t *arr;

    if (out == NULL) {
        return -1;
    }
    arr = xconf_find_array(cfg, key);
    if (arr == NULL || index >= arr->count) {
        return -1;
    }
    if (arr->items[index].type != XCONF_TYPE_NUMBER) {
        return -1;
    }
    *out = arr->items[index].as.number;
    return 0;
}

int xconf_get_string_element(xconf_t *cfg, const char *key, size_t index, const char **out)
{
    xconf_array_t *arr;

    if (out == NULL) {
        return -1;
    }
    arr = xconf_find_array(cfg, key);
    if (arr == NULL || index >= arr->count) {
        return -1;
    }
    if (arr->items[index].type != XCONF_TYPE_STRING) {
        return -1;
    }
    *out = arr->items[index].as.string;
    return 0;
}

int xconf_add_number(xconf_t *cfg, const char *key, double value)
{
    if (cfg == NULL) {
        return -1;
    }
    return xconf_document_upsert_number(cfg->doc, key, value);
}

int xconf_add_string(xconf_t *cfg, const char *key, const char *value)
{
    if (cfg == NULL) {
        return -1;
    }
    return xconf_document_upsert_string(cfg->doc, key, value);
}

int xconf_add_array_begin(xconf_t *cfg, const char *key)
{
    if (cfg == NULL) {
        return -1;
    }
    return xconf_document_upsert_array_begin(cfg->doc, key);
}

int xconf_array_push_number(xconf_t *cfg, const char *key, double value)
{
    if (cfg == NULL) {
        return -1;
    }
    return xconf_document_array_push_number(cfg->doc, key, value);
}

int xconf_array_push_string(xconf_t *cfg, const char *key, const char *value)
{
    if (cfg == NULL) {
        return -1;
    }
    return xconf_document_array_push_string(cfg->doc, key, value);
}

int xconf_save(xconf_t *cfg, const char *path)
{
    FILE *fp;
    int rc;

    if (cfg == NULL || path == NULL) {
        return -1;
    }

    /* Binary mode: the writer always emits a plain '\n', and binary
     * mode keeps that byte-for-byte on every platform (no CRLF
     * translation on Windows). */
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }

    rc = xconf_write_document(fp, cfg->doc);

    if (fclose(fp) != 0) {
        rc = -1;
    }

    return rc;
}

int xconf_foreach(xconf_t *cfg, xconf_foreach_cb callback, void *userdata)
{
    size_t i;

    if (cfg == NULL || callback == NULL) {
        return -1;
    }

    for (i = 0; i < cfg->doc->count; i++) {
        xconf_node_t *node = &cfg->doc->nodes[i];
        xconf_value_t value;
        int rc;

        if (node->kind != XCONF_NODE_ENTRY) {
            continue; /* comments/blank lines have no key/value */
        }
        if (xconf_document_find(cfg->doc, node->key) != i) {
            /* A later duplicate of this key overrides it (Phase 5);
             * skip this now-superseded occurrence rather than
             * visiting a stale value. */
            continue;
        }

        switch (node->value.type) {
            case XCONF_TYPE_NUMBER:
                value.type = XCONF_TYPE_NUMBER;
                value.as.number = node->value.as.number;
                break;
            case XCONF_TYPE_STRING:
                value.type = XCONF_TYPE_STRING;
                value.as.string = node->value.as.string;
                break;
            case XCONF_TYPE_ARRAY:
                value.type = XCONF_TYPE_ARRAY;
                value.as.array.count = node->value.as.array->count;
                value.as.array.items = node->value.as.array->items;
                break;
            case XCONF_TYPE_NONE:
            default:
                value.type = XCONF_TYPE_NONE;
                break;
        }

        rc = callback(node->key, &value, userdata);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}
