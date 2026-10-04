/*
 * test_phase7.c - full round-trip test: parse a sample file with
 * comments, blank lines, and a trailing comment; save it; reload the
 * saved file into a fresh xconf_t; and confirm both the values (via
 * the public query API) and the document structure (via
 * xconf_internal_document(), the test-only accessor from Phase 5)
 * match the original.
 *
 * This needs real parsing for both the initial parse AND the reload,
 * so - unlike test_writer.c - it cannot be meaningfully run without
 * flex/bison.
 */

#include "xconf.h"
#include "xconf_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_phase7: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

static void check_values(xconf_t *cfg, const char *label)
{
    double num;
    const char *str;
    size_t len;

    CHECK(xconf_get_number(cfg, "tax", &num) == 0 && num == 0.07, label);
    CHECK(xconf_get_string(cfg, "name", &str) == 0 && strcmp(str, "hello world") == 0, label);
    CHECK(xconf_get_array_length(cfg, "list", &len) == 0 && len == 3, label);
    CHECK(xconf_get_number_element(cfg, "list", 0, &num) == 0 && num == 1, label);
    CHECK(xconf_get_number_element(cfg, "list", 1, &num) == 0 && num == 2, label);
    CHECK(xconf_get_string_element(cfg, "list", 2, &str) == 0 && strcmp(str, "three") == 0, label);
}

int main(void)
{
    xconf_t *original;
    xconf_t *reloaded;
    xconf_document_t *doc1;
    xconf_document_t *doc2;
    const char *text =
        "# top comment\n"
        "\"tax\" = n(0.07)   # inline\n"
        "\"name\" = s(hello world)\n"
        "\"list\" = [1, 2, \"three\"]\n"
        "\n"
        "! trailing comment\n";
    const char *tmp_path = "test_phase7_output.tmp";

    original = xconf_init();
    CHECK(original != NULL, "xconf_init (original) returned NULL");
    CHECK(xconf_parse_string(original, text, strlen(text)) == 0, "parsing the sample text failed");

    check_values(original, "original document has the expected values");

    CHECK(xconf_save(original, tmp_path) == 0, "xconf_save failed");

    reloaded = xconf_init();
    CHECK(reloaded != NULL, "xconf_init (reloaded) returned NULL");
    CHECK(xconf_parse_file(reloaded, tmp_path) == 0, "reloading the saved file failed");

    check_values(reloaded, "reloaded document has the expected values");

    /* Structure: same node count and kinds, in the same order, with
     * comment text preserved verbatim. */
    doc1 = xconf_internal_document(original);
    doc2 = xconf_internal_document(reloaded);
    CHECK(doc1 != NULL && doc2 != NULL, "xconf_internal_document returned NULL");
    CHECK(doc1->count == doc2->count, "node count changed across save/reload");
    if (doc1 != NULL && doc2 != NULL && doc1->count == doc2->count) {
        size_t i;
        for (i = 0; i < doc1->count; i++) {
            CHECK(doc1->nodes[i].kind == doc2->nodes[i].kind, "node kind changed at some index");
            if (doc1->nodes[i].kind == XCONF_NODE_COMMENT) {
                CHECK(strcmp(doc1->nodes[i].comment_text, doc2->nodes[i].comment_text) == 0,
                      "comment text changed across save/reload");
            }
        }
    }

    xconf_free(original);
    xconf_free(reloaded);
    remove(tmp_path);

    if (failures == 0) {
        printf("test_phase7: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase7: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
