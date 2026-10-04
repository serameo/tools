/*
 * xconf_writer.h - serializes a document back to xconf-format text.
 * See xconf_phase7.md for the exact canonical rendering rules (bare
 * numbers, double-quoted strings, bracket arrays; comments and blank
 * lines reproduced verbatim).
 */

#ifndef XCONF_WRITER_H
#define XCONF_WRITER_H

#include <stdio.h>
#include "xconf_value.h"

/*
 * xconf_write_document - write doc to fp in xconf format. Returns 0
 * on success, -1 if a write error occurred (checked via ferror(fp)).
 * Does not open or close fp; the caller owns its lifetime.
 */
int xconf_write_document(FILE *fp, const xconf_document_t *doc);

#endif /* XCONF_WRITER_H */
