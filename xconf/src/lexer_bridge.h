/*
 * lexer_bridge.h - forward declarations for the flex-generated
 * in-memory scanning API, so xconf.c can drive the lexer/parser
 * without depending on flex's own (non-portable-to-include-twice)
 * generated header.
 *
 * These signatures match flex's default (non-reentrant) generated
 * code, which is what src/lexer.l currently produces (no "%option
 * reentrant"). If the lexer is ever switched to reentrant mode, this
 * file must be updated to match.
 */

#ifndef XCONF_LEXER_BRIDGE_H
#define XCONF_LEXER_BRIDGE_H

typedef struct yy_buffer_state *YY_BUFFER_STATE;

YY_BUFFER_STATE yy_scan_string(const char *yystr);
void yy_delete_buffer(YY_BUFFER_STATE b);

int yyparse(void);

#endif /* XCONF_LEXER_BRIDGE_H */
