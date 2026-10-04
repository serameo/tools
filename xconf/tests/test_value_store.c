/*
 * test_value_store.c - Phase 5 unit test for xconf_value.c only.
 *
 * Deliberately does not go through the lexer/parser, so this test can
 * be built and run even in environments where flex/bison are not
 * available (it links against xconf_value.c directly, not the
 * generated lexer/parser). Exercises the node-based document model:
 * add_number/add_string/add_array/add_comment/add_blank,
 * find-returns-last-match duplicate-key semantics, and correct
 * release of owned strings/arrays on destroy.
 */

#include "xconf_value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_value_store: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

/* Small helper: xconf_document_add_string/add_array/add_comment take
 * ownership of char* arguments, so tests must hand them freshly
 * malloc'd copies rather than string literals. */
static char *dupstr(const char *s)
{
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    memcpy(out, s, len + 1);
    return out;
}

int main(void)
{
    xconf_document_t *doc = xconf_document_create();
    size_t idx;
    double out;

    CHECK(doc != NULL, "xconf_document_create returned NULL");

    /* --- basic add + find ------------------------------------- */
    CHECK(xconf_document_add_number(doc, dupstr("tax"), 0.07, NULL) == 0,
          "add_number(tax) failed");
    idx = xconf_document_find(doc, "tax");
    CHECK(idx != (size_t)-1, "find(tax) did not find the key");
    CHECK(doc->nodes[idx].kind == XCONF_NODE_ENTRY, "tax node should be an entry");
    CHECK(doc->nodes[idx].value.as.number == 0.07, "tax value mismatch");

    /* Case-insensitive lookup. */
    idx = xconf_document_find(doc, "TAX");
    CHECK(idx != (size_t)-1, "find(TAX) should match case-insensitively");

    /* --- duplicate key: both entries kept, find() returns the LAST - */
    CHECK(xconf_document_add_number(doc, dupstr("Tax"), 0.10, NULL) == 0,
          "duplicate add_number(Tax) failed");
    CHECK(doc->count == 2, "a duplicate key must be appended, not merged in place");
    idx = xconf_document_find(doc, "tax");
    CHECK(idx == 1, "find() should return the LAST matching entry");
    out = doc->nodes[idx].value.as.number;
    CHECK(out == 0.10, "find() should resolve to the later value");
    /* ... but the first occurrence must still be there, unchanged. */
    CHECK(doc->nodes[0].value.as.number == 0.07, "the earlier duplicate must be preserved");

    /* --- strings, arrays, comments, blanks --------------------- */
    CHECK(xconf_document_add_string(doc, dupstr("appname"), dupstr("hello world"), NULL) == 0,
          "add_string(appname) failed");
    idx = xconf_document_find(doc, "APPNAME");
    CHECK(idx != (size_t)-1, "find(APPNAME) should match case-insensitively");
    CHECK(doc->nodes[idx].value.type == XCONF_TYPE_STRING, "appname should be a string");
    CHECK(strcmp(doc->nodes[idx].value.as.string, "hello world") == 0, "appname value mismatch");

    {
        xconf_array_t *arr = xconf_array_create();
        CHECK(arr != NULL, "xconf_array_create returned NULL");
        CHECK(xconf_array_append_number(arr, 123) == 0, "push_number(123) failed");
        CHECK(xconf_array_append_string(arr, dupstr("hello")) == 0, "push_string(hello) failed");
        CHECK(arr->count == 2, "expected two array elements");
        CHECK(xconf_document_add_array(doc, dupstr("mixed"), arr, dupstr("# a comment")) == 0,
              "add_array(mixed) failed");
    }
    idx = xconf_document_find(doc, "mixed");
    CHECK(idx != (size_t)-1, "find(mixed) failed");
    CHECK(doc->nodes[idx].value.type == XCONF_TYPE_ARRAY, "mixed should be an array");
    CHECK(doc->nodes[idx].value.as.array->count == 2, "mixed array should have 2 elements");
    CHECK(doc->nodes[idx].value.as.array->items[0].as.number == 123, "mixed[0] value mismatch");
    CHECK(strcmp(doc->nodes[idx].value.as.array->items[1].as.string, "hello") == 0,
          "mixed[1] value mismatch");
    CHECK(doc->nodes[idx].trailing_comment != NULL
          && strcmp(doc->nodes[idx].trailing_comment, "# a comment") == 0,
          "mixed trailing_comment mismatch");

    CHECK(xconf_document_add_comment(doc, dupstr("# a top-level comment")) == 0,
          "add_comment failed");
    CHECK(xconf_document_add_blank(doc) == 0, "add_blank failed");

    /* Comments/blanks are not ENTRY nodes, so they must never affect
     * key lookup. */
    idx = xconf_document_find(doc, "does_not_exist");
    CHECK(idx == (size_t)-1, "find() should not find a missing key");

    /* --- node kinds are as expected, in order ------------------- */
    CHECK(doc->count == 6, "expected 6 nodes total");
    CHECK(doc->nodes[0].kind == XCONF_NODE_ENTRY, "node 0 should be an entry (tax=0.07)");
    CHECK(doc->nodes[1].kind == XCONF_NODE_ENTRY, "node 1 should be an entry (tax=0.10)");
    CHECK(doc->nodes[2].kind == XCONF_NODE_ENTRY, "node 2 should be an entry (appname)");
    CHECK(doc->nodes[3].kind == XCONF_NODE_ENTRY, "node 3 should be an entry (mixed)");
    CHECK(doc->nodes[4].kind == XCONF_NODE_COMMENT, "node 4 should be a comment");
    CHECK(strcmp(doc->nodes[4].comment_text, "# a top-level comment") == 0,
          "comment text mismatch");
    CHECK(doc->nodes[5].kind == XCONF_NODE_BLANK, "node 5 should be a blank line");

    /* --- Phase 9: xconf_compose_origin_comment -------------------- */
    {
        char *result;

        /* origin_literal == NULL: existing_comment passes through
         * unchanged (the ordinary decimal/n(...) case). */
        result = xconf_compose_origin_comment(NULL, dupstr("# keep me"));
        CHECK(result != NULL && strcmp(result, "# keep me") == 0,
              "NULL origin should pass the existing comment through unchanged");
        free(result);

        result = xconf_compose_origin_comment(NULL, NULL);
        CHECK(result == NULL, "NULL origin and NULL existing comment should stay NULL");

        /* origin_literal set, no existing comment: a new comment is
         * created from just the origin note. */
        result = xconf_compose_origin_comment(dupstr("h(0x1234abcd)"), NULL);
        CHECK(result != NULL && strcmp(result, "# original: h(0x1234abcd)") == 0,
              "origin-only comment mismatch");
        free(result);

        /* origin_literal set AND an existing comment: both survive,
         * combined into one comment. */
        result = xconf_compose_origin_comment(dupstr("b(1010)"), dupstr("# my note"));
        CHECK(result != NULL && strcmp(result, "# original: b(1010) # my note") == 0,
              "origin + existing comment mismatch");
        free(result);
    }

    /* Destroying the document must not crash (frees every owned
     * string/array; a leak or double-free would only show up under a
     * memory checker, unavailable in this sandbox). */
    xconf_document_destroy(doc);

    if (failures == 0) {
        printf("test_value_store: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_value_store: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
