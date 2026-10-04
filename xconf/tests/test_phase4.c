/*
 * test_phase4.c - end-to-end test of the Phase 4 lexer/grammar via
 * xconf_parse_string(): both array forms, an empty array, and a
 * mixed number/string array.
 */

#include "xconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_phase4: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    xconf_t *cfg;
    const char *text =
        "\"a_form\"=a(n(123),s(hello),n(-1.23e6))\n"
        "\"bracket_form\"=[123, -1.23e6, \"hello\"]\n"
        "\"empty_a\"=a()\n"
        "\"empty_bracket\"=[]\n";
    size_t len;
    double num;
    const char *str;

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");

    CHECK(xconf_parse_string(cfg, text, strlen(text)) == 0, "xconf_parse_string failed");

    /* a(...) form: mixed number/string/number. */
    CHECK(xconf_get_value_type(cfg, "a_form") == XCONF_TYPE_ARRAY, "a_form should be an array");
    CHECK(xconf_get_array_length(cfg, "a_form", &len) == 0 && len == 3, "a_form length mismatch");
    CHECK(xconf_get_element_type(cfg, "a_form", 0) == XCONF_TYPE_NUMBER, "a_form[0] type mismatch");
    CHECK(xconf_get_number_element(cfg, "a_form", 0, &num) == 0 && num == 123,
          "a_form[0] value mismatch");
    CHECK(xconf_get_element_type(cfg, "a_form", 1) == XCONF_TYPE_STRING, "a_form[1] type mismatch");
    CHECK(xconf_get_string_element(cfg, "a_form", 1, &str) == 0 && strcmp(str, "hello") == 0,
          "a_form[1] value mismatch");
    CHECK(xconf_get_element_type(cfg, "a_form", 2) == XCONF_TYPE_NUMBER, "a_form[2] type mismatch");
    CHECK(xconf_get_number_element(cfg, "a_form", 2, &num) == 0 && num == -1.23e6,
          "a_form[2] value mismatch");

    /* [...] form: same values, using the bare/quoted syntax. */
    CHECK(xconf_get_value_type(cfg, "BRACKET_FORM") == XCONF_TYPE_ARRAY,
          "bracket_form should be an array (case-insensitive lookup)");
    CHECK(xconf_get_array_length(cfg, "bracket_form", &len) == 0 && len == 3,
          "bracket_form length mismatch");
    CHECK(xconf_get_number_element(cfg, "bracket_form", 0, &num) == 0 && num == 123,
          "bracket_form[0] value mismatch");
    CHECK(xconf_get_string_element(cfg, "bracket_form", 2, &str) == 0 && strcmp(str, "hello") == 0,
          "bracket_form[2] value mismatch");

    /* Empty arrays. */
    CHECK(xconf_get_value_type(cfg, "empty_a") == XCONF_TYPE_ARRAY, "empty_a should be an array");
    CHECK(xconf_get_array_length(cfg, "empty_a", &len) == 0 && len == 0, "empty_a should have length 0");
    CHECK(xconf_get_value_type(cfg, "empty_bracket") == XCONF_TYPE_ARRAY,
          "empty_bracket should be an array");
    CHECK(xconf_get_array_length(cfg, "empty_bracket", &len) == 0 && len == 0,
          "empty_bracket should have length 0");

    /* Out-of-range / wrong-type queries. */
    CHECK(xconf_get_element_type(cfg, "a_form", 99) == XCONF_TYPE_NONE,
          "out-of-range index should report XCONF_TYPE_NONE");
    CHECK(xconf_get_array_length(cfg, "does_not_exist", &len) == -1,
          "array length query on a missing key should fail");
    CHECK(xconf_get_number_element(cfg, "a_form", 1, &num) == -1,
          "fetching a string element as a number should fail");

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase4: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase4: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
