/*
 * test_phase10.c - tests xconf_foreach(): visits every key exactly
 * once (in document order), skips comment/blank lines, skips a
 * superseded duplicate key, exposes array values via the read-only
 * xconf_array_element_t view, and stops early when the callback
 * returns non-zero.
 */

#include "xconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_phase10: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

/* ---- pass 1: collect everything, confirm full traversal ---- */

typedef struct {
    char keys[8][32];
    xconf_type_t types[8];
    int count;
} collected_t;

static int collect_cb(const char *key, xconf_value_t *value, void *userdata)
{
    collected_t *out = (collected_t *)userdata;

    if (out->count >= 8) {
        return 0; /* shouldn't happen in this test; just guard the array */
    }
    strncpy(out->keys[out->count], key, sizeof(out->keys[0]) - 1);
    out->keys[out->count][sizeof(out->keys[0]) - 1] = '\0';
    out->types[out->count] = value->type;
    out->count++;
    return 0;
}

/* ---- pass 2: stop early on a specific key ---- */

static int stop_at_cb(const char *key, xconf_value_t *value, void *userdata)
{
    const char *stop_key = (const char *)userdata;
    (void)value;
    if (strcmp(key, stop_key) == 0) {
        return 42; /* arbitrary non-zero "stop here" code */
    }
    return 0;
}

/* ---- pass 3: check array contents via the read-only view ---- */

static int array_check_cb(const char *key, xconf_value_t *value, void *userdata)
{
    int *ok = (int *)userdata;

    if (strcmp(key, "list") != 0) {
        return 0;
    }
    if (value->type != XCONF_TYPE_ARRAY) {
        *ok = 0;
        return 0;
    }
    if (value->as.array.count != 3
        || value->as.array.items[0].type != XCONF_TYPE_NUMBER
        || value->as.array.items[0].as.number != 1
        || value->as.array.items[1].type != XCONF_TYPE_NUMBER
        || value->as.array.items[1].as.number != 2
        || value->as.array.items[2].type != XCONF_TYPE_STRING
        || strcmp(value->as.array.items[2].as.string, "three") != 0) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return 0;
}

int main(void)
{
    xconf_t *cfg;
    const char *text =
        "# a comment\n"
        "\"tax\" = 0.07\n"
        "\n"
        "\"tax\" = 0.10\n"
        "\"name\" = \"hello\"\n"
        "\"list\" = [1, 2, \"three\"]\n";

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");
    CHECK(xconf_parse_string(cfg, text, strlen(text)) == 0, "xconf_parse_string failed");

    /* Invalid arguments. */
    CHECK(xconf_foreach(NULL, collect_cb, NULL) == -1, "xconf_foreach(NULL, ...) should fail");
    CHECK(xconf_foreach(cfg, NULL, NULL) == -1, "xconf_foreach(cfg, NULL, ...) should fail");

    /* Full traversal: comment/blank lines skipped, superseded "tax"
     * (the first occurrence) skipped, three keys visited in order. */
    {
        collected_t collected;
        memset(&collected, 0, sizeof(collected));
        CHECK(xconf_foreach(cfg, collect_cb, &collected) == 0, "full traversal should return 0");
        CHECK(collected.count == 3, "expected exactly 3 keys (tax, name, list)");
        if (collected.count == 3) {
            CHECK(strcmp(collected.keys[0], "tax") == 0, "key 0 should be tax");
            CHECK(collected.types[0] == XCONF_TYPE_NUMBER, "tax should be a number");
            CHECK(strcmp(collected.keys[1], "name") == 0, "key 1 should be name");
            CHECK(collected.types[1] == XCONF_TYPE_STRING, "name should be a string");
            CHECK(strcmp(collected.keys[2], "list") == 0, "key 2 should be list");
            CHECK(collected.types[2] == XCONF_TYPE_ARRAY, "list should be an array");
        }
    }

    /* Stopping early: callback returns non-zero on "name", so
     * xconf_foreach must return that same value and never reach
     * "list". */
    CHECK(xconf_foreach(cfg, stop_at_cb, (void *)"name") == 42,
          "xconf_foreach should return the callback's non-zero return value");

    /* Array contents via the read-only view. */
    {
        int ok = 0;
        CHECK(xconf_foreach(cfg, array_check_cb, &ok) == 0, "array_check_cb pass should return 0");
        CHECK(ok == 1, "list array contents mismatch via xconf_foreach");
    }

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase10: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase10: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
