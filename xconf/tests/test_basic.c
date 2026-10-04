/*
 * test_basic.c - Phase 1 trivial test.
 *
 * Only checks that xconf_init() returns a non-NULL handle and that
 * xconf_free() does not crash. Real parsing/query tests are added
 * starting in Phase 2.
 */

#include "xconf.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    xconf_t *cfg = xconf_init();

    if (cfg == NULL) {
        fprintf(stderr, "test_basic: xconf_init() returned NULL\n");
        return EXIT_FAILURE;
    }

    xconf_free(cfg);

    printf("test_basic: OK\n");
    return EXIT_SUCCESS;
}
