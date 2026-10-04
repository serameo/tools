/*
 * test_phase3.c - end-to-end test of the Phase 3 lexer/grammar via
 * xconf_parse_string(): both string forms, escapes, and a multi-line
 * string value.
 */

#include "xconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_phase3: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    xconf_t *cfg;
    const char *text =
        "\"appname\"=s(hello world)\n"
        "\"say_hello\"=\"kon ni ji wa\"\n"
        "\"quoted\"=\"she said \\\"hi\\\"\"\n"
        "\"pathish\"=s(C:\\\\tmp)\n"
        "\"tabbed\"=\"a\\tb\"\n"
        "\"paren\"=s(a \\) b)\n"
        "\"multi\"=\"line one\n"
        "line two\"\n";
    const char *out;

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");

    CHECK(xconf_parse_string(cfg, text, strlen(text)) == 0, "xconf_parse_string failed");

    CHECK(xconf_get_value_type(cfg, "appname") == XCONF_TYPE_STRING, "appname should be a string");
    CHECK(xconf_get_string(cfg, "appname", &out) == 0 && strcmp(out, "hello world") == 0,
          "appname value mismatch");

    CHECK(xconf_get_string(cfg, "SAY_HELLO", &out) == 0 && strcmp(out, "kon ni ji wa") == 0,
          "say_hello value mismatch or case-insensitive lookup failed");

    CHECK(xconf_get_string(cfg, "quoted", &out) == 0 && strcmp(out, "she said \"hi\"") == 0,
          "quoted (escaped \\\") value mismatch");

    CHECK(xconf_get_string(cfg, "pathish", &out) == 0 && strcmp(out, "C:\\tmp") == 0,
          "pathish (escaped backslash) value mismatch");

    CHECK(xconf_get_string(cfg, "tabbed", &out) == 0 && strcmp(out, "a\tb") == 0,
          "tabbed (escaped \\t) value mismatch");

    CHECK(xconf_get_string(cfg, "paren", &out) == 0 && strcmp(out, "a ) b") == 0,
          "paren (escaped \\) inside s(...)) value mismatch");

    CHECK(xconf_get_string(cfg, "multi", &out) == 0 && strcmp(out, "line one\nline two") == 0,
          "multi (multi-line string) value mismatch");

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase3: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase3: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
