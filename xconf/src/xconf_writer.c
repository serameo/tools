/*
 * xconf_writer.c - see xconf_writer.h / xconf_phase7.md for the
 * canonical rendering rules this implements.
 */

#include "xconf_writer.h"
#include <stdlib.h>

/*
 * xconf_write_escaped_string - write s as a double-quoted xconf
 * string literal, escaping '"', '\\', newline, and tab. Used for both
 * keys and string values.
 */
static void xconf_write_escaped_string(FILE *fp, const char *s)
{
    fputc('"', fp);
    for (; *s != '\0'; s++) {
        switch (*s) {
            case '"':  fputs("\\\"", fp); break;
            case '\\': fputs("\\\\", fp); break;
            case '\n': fputs("\\n", fp);  break;
            case '\t': fputs("\\t", fp);  break;
            default:   fputc(*s, fp);     break;
        }
    }
    fputc('"', fp);
}

/*
 * xconf_write_number - write value using the shortest of %.15g /
 * %.17g that round-trips back to the exact same double. %.15g is
 * enough for the vast majority of values and reads cleanly (e.g.
 * "0.07"); %.17g is the guaranteed-round-trip fallback for the rare
 * value it is not enough for.
 */
static void xconf_write_number(FILE *fp, double value)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "%.15g", value);
    if (atof(buf) != value) {
        snprintf(buf, sizeof(buf), "%.17g", value);
    }
    fputs(buf, fp);
}

static void xconf_write_array(FILE *fp, const xconf_array_t *arr)
{
    size_t i;

    fputc('[', fp);
    for (i = 0; i < arr->count; i++) {
        if (i > 0) {
            fputs(", ", fp);
        }
        if (arr->items[i].type == XCONF_TYPE_NUMBER) {
            xconf_write_number(fp, arr->items[i].as.number);
        } else {
            xconf_write_escaped_string(fp, arr->items[i].as.string);
        }
    }
    fputc(']', fp);
}

static void xconf_write_value(FILE *fp, const xconf_node_value_t *value)
{
    switch (value->type) {
        case XCONF_TYPE_NUMBER:
            xconf_write_number(fp, value->as.number);
            break;
        case XCONF_TYPE_STRING:
            xconf_write_escaped_string(fp, value->as.string);
            break;
        case XCONF_TYPE_ARRAY:
            xconf_write_array(fp, value->as.array);
            break;
        case XCONF_TYPE_NONE:
            /* Should not happen for an ENTRY node; write nothing. */
            break;
    }
}

int xconf_write_document(FILE *fp, const xconf_document_t *doc)
{
    size_t i;

    if (fp == NULL || doc == NULL) {
        return -1;
    }

    for (i = 0; i < doc->count; i++) {
        const xconf_node_t *node = &doc->nodes[i];

        switch (node->kind) {
            case XCONF_NODE_ENTRY:
                xconf_write_escaped_string(fp, node->key);
                fputs(" = ", fp);
                xconf_write_value(fp, &node->value);
                if (node->trailing_comment != NULL) {
                    fputc(' ', fp);
                    fputs(node->trailing_comment, fp);
                }
                fputc('\n', fp);
                break;
            case XCONF_NODE_COMMENT:
                fputs(node->comment_text, fp);
                fputc('\n', fp);
                break;
            case XCONF_NODE_BLANK:
                fputc('\n', fp);
                break;
        }
    }

    return ferror(fp) ? -1 : 0;
}
