/*
 * xconf_internal.h - private declarations shared between xconf.c and
 * the generated parser/lexer (parser.y / lexer.l).
 *
 * KNOWN LIMITATION: the flex/bison pipeline used here is the default
 * non-reentrant one, so xconf_current_parse_target and
 * xconf_lexer_depth are single global variables. This means
 * xconf_parse_string()/xconf_parse_file() are not safe to call
 * concurrently from multiple threads. This is acceptable for the
 * library's intended single-threaded use (see xcomf_plans.md,
 * section 3, Non-Goals). Switching to a reentrant (%define api.pure)
 * parser can be revisited later if needed.
 */

#ifndef XCONF_INTERNAL_H
#define XCONF_INTERNAL_H

#include "xconf_value.h"

/* Set by xconf_parse_string()/xconf_parse_file() before calling
 * yyparse(), so grammar actions know which document to populate. */
extern xconf_document_t *xconf_current_parse_target;

/* Paren/bracket nesting depth, maintained by lexer.l: 0 at the top
 * level, >0 while inside an a(...)/[...] array or an n(...) number
 * wrapper. Used to decide whether a newline or comment is significant
 * (top level) or just whitespace to skip (nested). Reset to 0 by
 * xconf_parse_string() before each parse. */
extern int xconf_lexer_depth;

/*
 * xconf_internal_document - test-only accessor to the xconf_document_t
 * backing an xconf_t. Not part of the public API (not declared in
 * xconf.h); used by tests that need to inspect node structure
 * directly (comments/blank lines have no public query functions).
 */
xconf_document_t *xconf_internal_document(xconf_t *cfg);

#endif /* XCONF_INTERNAL_H */
