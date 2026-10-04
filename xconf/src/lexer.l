/*
 * lexer.l - flex input for the xconf lexer.
 *
 * Phase 5: comments and newlines become significant tokens (COMMENT,
 * NEWLINE) at the top level, so the grammar can build the
 * format-preserving document model (see xconf_value.h). A paren/
 * bracket nesting depth (xconf_lexer_depth, declared in
 * xconf_internal.h) tracks whether we are inside an a(...)/[...]
 * array or an n(...) number wrapper: while depth > 0, a newline is
 * just whitespace (so multi-line arrays keep working) and a comment
 * is dropped entirely rather than tokenized (see xconf_phase5.md for
 * why this is an accepted limitation).
 *
 * Phase 9: adds h(...)/b(...) hexadecimal/binary number literals.
 * Each is matched whole by its own rule (never going through the
 * standalone '('/')' rules below), so they do not interact with the
 * depth counter - they cannot themselves contain a newline or a
 * nested array/number wrapper, so there is nothing for the depth
 * counter to track inside them.
 */

%{
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"
#include "xconf_internal.h"

/*
 * xconf_lexer_unescape - copy `raw_len` bytes starting at `raw` into
 * a new, NUL-terminated buffer, applying backslash escaping:
 *   \"  -> "
 *   \\  -> backslash
 *   \n  -> newline
 *   \t  -> tab
 *   \)  -> )        (only meaningful inside s(...), harmless elsewhere)
 * An unrecognized escape (e.g. \z) is kept as-is (backslash and the
 * following character), rather than treated as an error.
 */
static char *xconf_lexer_unescape(const char *raw, size_t raw_len)
{
    char *out;
    size_t i, oi;

    out = (char *)malloc(raw_len + 1);
    if (out == NULL) {
        return NULL;
    }

    i = 0;
    oi = 0;
    while (i < raw_len) {
        if (raw[i] == '\\' && i + 1 < raw_len) {
            char next = raw[i + 1];
            switch (next) {
                case '"':  out[oi++] = '"';  break;
                case '\\': out[oi++] = '\\'; break;
                case 'n':  out[oi++] = '\n'; break;
                case 't':  out[oi++] = '\t'; break;
                case ')':  out[oi++] = ')';  break;
                default:
                    out[oi++] = '\\';
                    out[oi++] = next;
                    break;
            }
            i += 2;
        } else {
            out[oi++] = raw[i];
            i += 1;
        }
    }
    out[oi] = '\0';
    return out;
}

/*
 * xconf_lexer_dup - plain (non-unescaping) copy of len bytes starting
 * at s, NUL-terminated. Used for comment text, which is stored
 * verbatim (including its '#'/'!' marker) and never unescaped.
 */
static char *xconf_lexer_dup(const char *s, size_t len)
{
    char *out = (char *)malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/*
 * xconf_lexer_parse_based_number - Phase 9: parse the digits between
 * an h(...)/b(...) literal's parentheses into a double. `text`/`len`
 * cover just that inner span (e.g. "-0x1234abcd" or "-1010"), not the
 * "h("/"b("/")" wrapper. `base` is 16 or 2. An optional leading '-' is
 * handled first; for base 16, an optional "0x"/"0X" prefix is then
 * skipped. Digits beyond that are assumed valid (the lexer's own
 * regex already validated the character set) - this function only
 * does the base conversion.
 */
static double xconf_lexer_parse_based_number(const char *text, size_t len, int base)
{
    int negative = 0;
    size_t i = 0;
    unsigned long long magnitude = 0;

    if (i < len && text[i] == '-') {
        negative = 1;
        i++;
    }
    if (base == 16 && i + 1 < len && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        i += 2;
    }
    for (; i < len; i++) {
        char c = text[i];
        int digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        } else {
            break; /* not reached given the lexer's own regex */
        }
        magnitude = magnitude * (unsigned long long)base + (unsigned long long)digit;
    }

    return negative ? -(double)magnitude : (double)magnitude;
}
%}

%option noyywrap
%option nounput
%option noinput
/* Phase 8: nounistd stops flex emitting "#include <unistd.h>" (that
 * header does not exist on MSVC - this is the single most common
 * real-world flex+Windows build failure); never-interactive stops it
 * emitting isatty()/fileno() calls for interactive-terminal detection
 * (POSIX-only, and irrelevant here anyway since xconf only ever scans
 * from an in-memory buffer via yy_scan_string(), never from yyin). */
%option nounistd
%option never-interactive

%%

"#"[^\n]*                              {
                                            if (xconf_lexer_depth == 0) {
                                                yylval.sval = xconf_lexer_dup(yytext, (size_t)yyleng);
                                                return COMMENT;
                                            }
                                            /* inside an array/number wrapper: dropped */
                                        }
"!"[^\n]*                              {
                                            if (xconf_lexer_depth == 0) {
                                                yylval.sval = xconf_lexer_dup(yytext, (size_t)yyleng);
                                                return COMMENT;
                                            }
                                        }
\n                                      {
                                            if (xconf_lexer_depth == 0) {
                                                return NEWLINE;
                                            }
                                            /* inside an array/number wrapper: just whitespace */
                                        }
[ \t\r]+                                { /* horizontal whitespace: always ignored */ }

\"([^"\\]|\\.)*\"                      {
                                            yylval.sval = xconf_lexer_unescape(yytext + 1, (size_t)(yyleng - 2));
                                            return QSTRING;
                                        }

s\(([^)\\]|\\.)*\)                     {
                                            yylval.sval = xconf_lexer_unescape(yytext + 2, (size_t)(yyleng - 3));
                                            return SSTRING;
                                        }

h\(-?(0[xX])?[0-9a-fA-F]+\)            {
                                            yylval.numlit.value =
                                                xconf_lexer_parse_based_number(yytext + 2, (size_t)(yyleng - 3), 16);
                                            yylval.numlit.origin = xconf_lexer_dup(yytext, (size_t)yyleng);
                                            return BASENUM;
                                        }

b\(-?[01]+\)                           {
                                            yylval.numlit.value =
                                                xconf_lexer_parse_based_number(yytext + 2, (size_t)(yyleng - 3), 2);
                                            yylval.numlit.origin = xconf_lexer_dup(yytext, (size_t)yyleng);
                                            return BASENUM;
                                        }

-?[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?  {
                                            yylval.dval = atof(yytext);
                                            return NUMBER;
                                        }

"n"                                     { return 'n'; }
"a"                                     { return 'a'; }
"="                                     { return '='; }
"("                                     { xconf_lexer_depth++; return '('; }
")"                                     { xconf_lexer_depth--; return ')'; }
"["                                     { xconf_lexer_depth++; return '['; }
"]"                                     { xconf_lexer_depth--; return ']'; }
","                                     { return ','; }

.                                       { /* any other character: skipped
                                             for now. Proper error
                                             reporting for unexpected
                                             characters is a follow-up,
                                             consistent with earlier
                                             phases. */ }

%%
