/*
 * test_writer.c - tests xconf_write_document() directly, building the
 * document via the internal xconf_document_add_* functions (no
 * parser involved), so this can be fully compiled AND RUN even where
 * flex/bison are unavailable.
 */

#include "xconf_value.h"
#include "xconf_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "test_writer: FAILED: %s\n", (msg)); \
            failures++; \
        } \
    } while (0)

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
    FILE *fp;
    char buf[4096];
    size_t n;

    CHECK(doc != NULL, "xconf_document_create returned NULL");

    CHECK(xconf_document_add_comment(doc, dupstr("# a comment")) == 0, "add_comment failed");
    CHECK(xconf_document_add_number(doc, dupstr("tax"), 0.07, dupstr("# inline")) == 0,
          "add_number(tax) failed");
    CHECK(xconf_document_add_blank(doc) == 0, "add_blank failed");
    CHECK(xconf_document_add_string(doc, dupstr("name"), dupstr("hello \"world\""), NULL) == 0,
          "add_string(name) failed");
    {
        xconf_array_t *arr = xconf_array_create();
        CHECK(arr != NULL, "xconf_array_create returned NULL");
        CHECK(xconf_array_append_number(arr, 1) == 0, "append_number(1) failed");
        CHECK(xconf_array_append_string(arr, dupstr("two")) == 0, "append_string(two) failed");
        CHECK(xconf_document_add_array(doc, dupstr("list"), arr, NULL) == 0, "add_array(list) failed");
    }

    fp = tmpfile();
    CHECK(fp != NULL, "tmpfile() failed - cannot continue this test without it");
    if (fp == NULL) {
        xconf_document_destroy(doc);
        return EXIT_FAILURE;
    }

    CHECK(xconf_write_document(fp, doc) == 0, "xconf_write_document failed");

    rewind(fp);
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);

    CHECK(strstr(buf, "# a comment\n") != NULL, "comment line missing or wrong");
    CHECK(strstr(buf, "\"tax\" = 0.07 # inline\n") != NULL,
          "tax line mismatch (number formatting or trailing comment placement)");
    CHECK(strstr(buf, "\n\n") != NULL, "blank line missing");
    CHECK(strstr(buf, "\"name\" = \"hello \\\"world\\\"\"\n") != NULL,
          "name line mismatch (string escaping)");
    CHECK(strstr(buf, "\"list\" = [1, \"two\"]\n") != NULL,
          "list line mismatch (array formatting)");

    /* Order matters too: the comment must come before "tax", which
     * must come before the blank line, and so on. */
    {
        const char *p_comment = strstr(buf, "# a comment");
        const char *p_tax = strstr(buf, "\"tax\"");
        const char *p_name = strstr(buf, "\"name\"");
        const char *p_list = strstr(buf, "\"list\"");
        CHECK(p_comment != NULL && p_tax != NULL && p_comment < p_tax,
              "comment should come before tax");
        CHECK(p_tax != NULL && p_name != NULL && p_tax < p_name,
              "tax should come before name");
        CHECK(p_name != NULL && p_list != NULL && p_name < p_list,
              "name should come before list");
    }

    xconf_document_destroy(doc);

    if (failures == 0) {
        printf("test_writer: OK\n");
        return EXIT_SUCCESS;
    }
    fprintf(stderr, "test_writer: %d check(s) failed\n", failures);
    return EXIT_FAILURE;
}
