/*
 * test_phase2.c - end-to-end test of the Phase 2 lexer/grammar via
 * xconf_parse_string(): comments, both number forms, duplicate-key
 * overwrite, and case-insensitive lookup.
 */

#include "xconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_phase2: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    xconf_t *cfg;
    const char *text =
        "# this is a comment\n"
        "! this is also a comment\n"
        "\"tax\" = n(0.07)   # inline comment after a value\n"
        "\"pi\"=3.14e7\n"
        "\"neg\" = n(-1.23E-5)\n"
        "\"dup\" = n(1)\n"
        "\"DUP\" = n(2)\n"; /* duplicate key, different case: must overwrite */
    double out;

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");

    CHECK(xconf_parse_string(cfg, text, strlen(text)) == 0, "xconf_parse_string failed");

    CHECK(xconf_get_value_type(cfg, "tax") == XCONF_TYPE_NUMBER, "tax should be a number");
    CHECK(xconf_get_number(cfg, "tax", &out) == 0 && out == 0.07, "tax value mismatch");

    CHECK(xconf_get_number(cfg, "PI", &out) == 0 && out == 3.14e7,
          "pi value mismatch or case-insensitive lookup failed");

    CHECK(xconf_get_number(cfg, "neg", &out) == 0 && out == -1.23E-5, "neg value mismatch");

    CHECK(xconf_get_number(cfg, "dup", &out) == 0 && out == 2,
          "duplicate key (different case) should overwrite to the later value");

    CHECK(xconf_get_value_type(cfg, "missing") == XCONF_TYPE_NONE,
          "missing key should report XCONF_TYPE_NONE");

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase2: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase2: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
