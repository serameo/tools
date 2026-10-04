/*
 * test_phase5.c - end-to-end test of the Phase 5 grammar via
 * xconf_parse_string(): comments, blank lines, a same-line trailing
 * comment, and duplicate-key resolution, inspected both through the
 * public query API and through the internal node list (via
 * xconf_internal_document(), a test-only accessor - see
 * xconf_internal.h).
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
            fprintf(stderr, "test_phase5: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    xconf_t *cfg;
    xconf_document_t *doc;
    const char *text =
        "# top comment\n"
        "\"tax\" = n(0.07)   # inline\n"
        "\n"
        "\"tax\" = n(0.10)\n"
        "! another comment\n"
        "\"pi\"=3.14\n";
    double num;

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");

    CHECK(xconf_parse_string(cfg, text, strlen(text)) == 0, "xconf_parse_string failed");

    /* Public API: duplicate key resolves to the later value. */
    CHECK(xconf_get_number(cfg, "tax", &num) == 0 && num == 0.10,
          "tax should resolve to the later (0.10) value");
    CHECK(xconf_get_number(cfg, "PI", &num) == 0 && num == 3.14,
          "pi value mismatch or case-insensitive lookup failed");

    /* Internal node list: nothing from the source was discarded. */
    doc = xconf_internal_document(cfg);
    CHECK(doc != NULL, "xconf_internal_document returned NULL");
    CHECK(doc->count == 6, "expected 6 nodes (comment, tax=0.07, blank, tax=0.10, comment, pi)");

    CHECK(doc->nodes[0].kind == XCONF_NODE_COMMENT, "node 0 should be a comment");
    CHECK(strcmp(doc->nodes[0].comment_text, "# top comment") == 0, "node 0 text mismatch");

    CHECK(doc->nodes[1].kind == XCONF_NODE_ENTRY, "node 1 should be an entry");
    CHECK(strcmp(doc->nodes[1].key, "tax") == 0, "node 1 key mismatch");
    CHECK(doc->nodes[1].value.as.number == 0.07, "node 1 value mismatch");
    CHECK(doc->nodes[1].trailing_comment != NULL
          && strcmp(doc->nodes[1].trailing_comment, "# inline") == 0,
          "node 1 trailing_comment mismatch");

    CHECK(doc->nodes[2].kind == XCONF_NODE_BLANK, "node 2 should be a blank line");

    CHECK(doc->nodes[3].kind == XCONF_NODE_ENTRY, "node 3 should be an entry");
    CHECK(strcmp(doc->nodes[3].key, "tax") == 0, "node 3 key mismatch");
    CHECK(doc->nodes[3].value.as.number == 0.10, "node 3 value mismatch");
    CHECK(doc->nodes[3].trailing_comment == NULL, "node 3 should have no trailing comment");

    CHECK(doc->nodes[4].kind == XCONF_NODE_COMMENT, "node 4 should be a comment");
    CHECK(strcmp(doc->nodes[4].comment_text, "! another comment") == 0, "node 4 text mismatch");

    CHECK(doc->nodes[5].kind == XCONF_NODE_ENTRY, "node 5 should be an entry");
    CHECK(strcmp(doc->nodes[5].key, "pi") == 0, "node 5 key mismatch");

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase5: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase5: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
