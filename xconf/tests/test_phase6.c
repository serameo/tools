/*
 * test_phase6.c - tests the programmatic add API (xconf_add_number,
 * xconf_add_string, xconf_add_array_begin + xconf_array_push_*),
 * built entirely without calling xconf_parse_*. Also confirms
 * insert-or-update-in-place semantics using xconf_internal_document()
 * (the same test-only accessor introduced in Phase 5).
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
            fprintf(stderr, "test_phase6: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    xconf_t *cfg;
    xconf_document_t *doc;
    double num;
    const char *str;
    size_t len;

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");

    /* Build a document from scratch: no xconf_parse_* call at all. */
    CHECK(xconf_add_number(cfg, "tax", 0.07) == 0, "add_number(tax) failed");
    CHECK(xconf_add_string(cfg, "appname", "hello world") == 0, "add_string(appname) failed");
    CHECK(xconf_add_array_begin(cfg, "mixed") == 0, "add_array_begin(mixed) failed");
    CHECK(xconf_array_push_number(cfg, "mixed", 1) == 0, "array_push_number(mixed, 1) failed");
    CHECK(xconf_array_push_string(cfg, "mixed", "two") == 0, "array_push_string(mixed, two) failed");
    CHECK(xconf_array_push_number(cfg, "mixed", 3) == 0, "array_push_number(mixed, 3) failed");

    /* Round-trip through the query API. */
    CHECK(xconf_get_value_type(cfg, "tax") == XCONF_TYPE_NUMBER, "tax should be a number");
    CHECK(xconf_get_number(cfg, "TAX", &num) == 0 && num == 0.07,
          "tax value mismatch or case-insensitive lookup failed");

    CHECK(xconf_get_value_type(cfg, "appname") == XCONF_TYPE_STRING, "appname should be a string");
    CHECK(xconf_get_string(cfg, "appname", &str) == 0 && strcmp(str, "hello world") == 0,
          "appname value mismatch");

    CHECK(xconf_get_value_type(cfg, "mixed") == XCONF_TYPE_ARRAY, "mixed should be an array");
    CHECK(xconf_get_array_length(cfg, "mixed", &len) == 0 && len == 3, "mixed length mismatch");
    CHECK(xconf_get_number_element(cfg, "mixed", 0, &num) == 0 && num == 1, "mixed[0] value mismatch");
    CHECK(xconf_get_string_element(cfg, "mixed", 1, &str) == 0 && strcmp(str, "two") == 0,
          "mixed[1] value mismatch");
    CHECK(xconf_get_number_element(cfg, "mixed", 2, &num) == 0 && num == 3, "mixed[2] value mismatch");

    /* Pushing onto a key that is not an array (or does not exist)
     * must fail cleanly rather than corrupt anything. */
    CHECK(xconf_array_push_number(cfg, "tax", 1) == -1,
          "pushing onto a non-array key should fail");
    CHECK(xconf_array_push_number(cfg, "does_not_exist", 1) == -1,
          "pushing onto a missing key should fail");

    /* Node count so far: tax, appname, mixed = 3 entries, no comments
     * or blanks since nothing was parsed. */
    doc = xconf_internal_document(cfg);
    CHECK(doc != NULL, "xconf_internal_document returned NULL");
    CHECK(doc->count == 3, "expected exactly 3 nodes after the adds above");

    /* Re-adding an existing key must update it IN PLACE (no new
     * node), unlike a duplicate key encountered while parsing. */
    CHECK(xconf_add_number(cfg, "tax", 0.10) == 0, "re-adding tax failed");
    CHECK(doc->count == 3, "updating an existing key must not add a node");
    CHECK(xconf_get_number(cfg, "tax", &num) == 0 && num == 0.10, "tax should now be 0.10");

    /* A genuinely new key does append a node. */
    CHECK(xconf_add_number(cfg, "pi", 3.14) == 0, "add_number(pi) failed");
    CHECK(doc->count == 4, "adding a new key should append a node");

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase6: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase6: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
