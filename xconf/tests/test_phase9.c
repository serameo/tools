/*
 * test_phase9.c - end-to-end test of Phase 9's h(...)/b(...) number
 * literals via xconf_parse_string(): all three hex prefix forms,
 * binary, negative values, use inside both array forms, and the
 * origin-preserving comment added for a top-level hex/binary entry
 * (checked via xconf_internal_document(), the test-only accessor from
 * Phase 5).
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
            fprintf(stderr, "test_phase9: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    xconf_t *cfg;
    xconf_document_t *doc;
    const char *text =
        "\"hex_a\" = h(0x1234abcd)\n"
        "\"hex_b\" = h(0X1234ABCD)\n"
        "\"hex_c\" = h(1234abcd)\n"
        "\"bin_a\" = b(11010011)\n"
        "\"neg_hex\" = h(-0x10)\n"
        "\"neg_bin\" = b(-1010)\n"
        "\"arr\" = a(h(0x10), b(101), n(3))\n"
        "\"barr\" = [h(0x20), 5, \"x\"]\n"
        "\"plain\" = h(0xff)\n"
        "\"commented\" = h(0xff) # my note\n";
    double num;
    size_t idx;

    cfg = xconf_init();
    CHECK(cfg != NULL, "xconf_init returned NULL");
    CHECK(xconf_parse_string(cfg, text, strlen(text)) == 0, "xconf_parse_string failed");

    /* All three hex prefix forms parse to the same value. */
    {
        double expected = (double)0x1234abcdLL;
        CHECK(xconf_get_number(cfg, "hex_a", &num) == 0 && num == expected, "hex_a value mismatch");
        CHECK(xconf_get_number(cfg, "hex_b", &num) == 0 && num == expected, "hex_b value mismatch");
        CHECK(xconf_get_number(cfg, "hex_c", &num) == 0 && num == expected, "hex_c value mismatch");
    }

    /* Binary. */
    CHECK(xconf_get_number(cfg, "bin_a", &num) == 0 && num == 211, "bin_a value mismatch (expected 211)");

    /* Negative forms. */
    CHECK(xconf_get_number(cfg, "neg_hex", &num) == 0 && num == -16, "neg_hex value mismatch");
    CHECK(xconf_get_number(cfg, "neg_bin", &num) == 0 && num == -10, "neg_bin value mismatch");

    /* Inside a(...): h(0x10)=16, b(101)=5, n(3)=3. */
    CHECK(xconf_get_number_element(cfg, "arr", 0, &num) == 0 && num == 16, "arr[0] value mismatch");
    CHECK(xconf_get_number_element(cfg, "arr", 1, &num) == 0 && num == 5, "arr[1] value mismatch");
    CHECK(xconf_get_number_element(cfg, "arr", 2, &num) == 0 && num == 3, "arr[2] value mismatch");

    /* Inside [...]: h(0x20)=32, bare 5, "x". */
    {
        const char *str;
        CHECK(xconf_get_number_element(cfg, "barr", 0, &num) == 0 && num == 32, "barr[0] value mismatch");
        CHECK(xconf_get_number_element(cfg, "barr", 1, &num) == 0 && num == 5, "barr[1] value mismatch");
        CHECK(xconf_get_string_element(cfg, "barr", 2, &str) == 0 && strcmp(str, "x") == 0,
              "barr[2] value mismatch");
    }

    /* Origin-preserving comment: a top-level hex/binary entry gets a
     * "(originally hex/binary)" note; an array element does not (see
     * xconf_phase9.md's Scoping Note). */
    doc = xconf_internal_document(cfg);
    CHECK(doc != NULL, "xconf_internal_document returned NULL");

    idx = xconf_document_find(doc, "plain");
    CHECK(idx != (size_t)-1, "find(plain) failed");
    if (idx != (size_t)-1) {
        CHECK(doc->nodes[idx].trailing_comment != NULL
              && strcmp(doc->nodes[idx].trailing_comment, "# original: h(0xff)") == 0,
              "plain should have gained an origin comment with no prior comment");
    }

    idx = xconf_document_find(doc, "commented");
    CHECK(idx != (size_t)-1, "find(commented) failed");
    if (idx != (size_t)-1) {
        CHECK(doc->nodes[idx].trailing_comment != NULL
              && strcmp(doc->nodes[idx].trailing_comment, "# original: h(0xff) # my note") == 0,
              "commented should combine the origin note with the existing comment");
    }

    idx = xconf_document_find(doc, "arr");
    CHECK(idx != (size_t)-1, "find(arr) failed");
    if (idx != (size_t)-1) {
        CHECK(doc->nodes[idx].trailing_comment == NULL,
              "an entry whose VALUE is an array must not get an origin comment, "
              "even though some of its elements came from h()/b()");
    }

    xconf_free(cfg);

    if (failures == 0) {
        printf("test_phase9: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_phase9: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
