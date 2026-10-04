/*
 * parser.y - bison grammar for xconf.
 *
 * Phase 5: restructured around lines rather than bare entries, so
 * comments and blank lines become their own document nodes (see
 * xconf_value.h). A "line" is one of: a blank line (bare NEWLINE), a
 * comment-only line, or a "KEY = value" entry with an optional
 * same-line trailing comment. "line_end" allows the final line of the
 * file to omit its trailing newline.
 *
 * KNOWN LIMITATION: there is no bison %destructor yet, so a syntax
 * error partway through a line (especially inside an array) leaks
 * whatever was already built for it. Consistent with Phases 2-4,
 * which also do not handle parse-error cleanup; left as a follow-up.
 *
 * Phase 9: number_value's type is now xconf_numlit_t (value + an
 * optional origin string) instead of a plain double, so a hex/binary
 * literal's original source text can ride along to the point where it
 * is combined with any trailing comment (see xconf_compose_origin_comment
 * in xconf_value.h). Array elements (a_element/bracket_element) accept
 * BASENUM too, but only use its .value - see xconf_phase9.md's Scoping
 * Note for why no origin comment is attached for array elements.
 */

%code requires {
#include "xconf_value.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include "xconf_internal.h"

int yylex(void);
void yyerror(const char *msg);
%}

%union {
    double dval;
    char  *sval;
    xconf_array_t *arrval;
    xconf_array_element_t scalarval;
    xconf_numlit_t numlit;
}

%token <sval> QSTRING
%token <sval> SSTRING
%token <sval> COMMENT
%token <dval> NUMBER
%token <numlit> BASENUM
%token NEWLINE

%type <numlit> number_value
%type <sval> string_value
%type <sval> trailing_comment
%type <arrval> array_value
%type <arrval> a_element_list
%type <arrval> a_element_list_opt
%type <arrval> bracket_element_list
%type <arrval> bracket_element_list_opt
%type <scalarval> a_element
%type <scalarval> bracket_element

%%

document
    : /* empty */
    | document line
    ;

line_end
    : NEWLINE
    | /* empty: only valid at end-of-file */
    ;

trailing_comment
    : /* empty */    { $$ = NULL; }
    | COMMENT        { $$ = $1; }
    ;

line
    : NEWLINE
        {
            xconf_document_add_blank(xconf_current_parse_target);
        }
    | COMMENT line_end
        {
            xconf_document_add_comment(xconf_current_parse_target, $1);
        }
    | QSTRING '=' number_value trailing_comment line_end
        {
            char *comment = xconf_compose_origin_comment($3.origin, $4);
            xconf_document_add_number(xconf_current_parse_target, $1, $3.value, comment);
        }
    | QSTRING '=' string_value trailing_comment line_end
        {
            xconf_document_add_string(xconf_current_parse_target, $1, $3, $4);
        }
    | QSTRING '=' array_value trailing_comment line_end
        {
            xconf_document_add_array(xconf_current_parse_target, $1, $3, $4);
        }
    ;

number_value
    : NUMBER                { $$.value = $1; $$.origin = NULL; }
    | 'n' '(' NUMBER ')'     { $$.value = $3; $$.origin = NULL; }
    | BASENUM                { $$ = $1; }
    ;

string_value
    : QSTRING                { $$ = $1; }
    | SSTRING                { $$ = $1; }
    ;

array_value
    : 'a' '(' a_element_list_opt ')'      { $$ = $3; }
    | '[' bracket_element_list_opt ']'    { $$ = $2; }
    ;

a_element_list_opt
    : /* empty */       { $$ = xconf_array_create(); }
    | a_element_list    { $$ = $1; }
    ;

a_element_list
    : a_element
        {
            $$ = xconf_array_create();
            if ($1.type == XCONF_TYPE_NUMBER) {
                xconf_array_append_number($$, $1.as.number);
            } else {
                xconf_array_append_string($$, $1.as.string);
            }
        }
    | a_element_list ',' a_element
        {
            if ($3.type == XCONF_TYPE_NUMBER) {
                xconf_array_append_number($1, $3.as.number);
            } else {
                xconf_array_append_string($1, $3.as.string);
            }
            $$ = $1;
        }
    ;

a_element
    : 'n' '(' NUMBER ')'
        {
            $$.type = XCONF_TYPE_NUMBER;
            $$.as.number = $3;
        }
    | BASENUM
        {
            /* Array elements don't have a slot for a trailing comment
             * (see xconf_phase9.md's Scoping Note), so only the value
             * is kept here; the origin text is discarded. */
            $$.type = XCONF_TYPE_NUMBER;
            $$.as.number = $1.value;
            free($1.origin);
        }
    | SSTRING
        {
            $$.type = XCONF_TYPE_STRING;
            $$.as.string = $1; /* ownership moves to the array */
        }
    ;

bracket_element_list_opt
    : /* empty */             { $$ = xconf_array_create(); }
    | bracket_element_list    { $$ = $1; }
    ;

bracket_element_list
    : bracket_element
        {
            $$ = xconf_array_create();
            if ($1.type == XCONF_TYPE_NUMBER) {
                xconf_array_append_number($$, $1.as.number);
            } else {
                xconf_array_append_string($$, $1.as.string);
            }
        }
    | bracket_element_list ',' bracket_element
        {
            if ($3.type == XCONF_TYPE_NUMBER) {
                xconf_array_append_number($1, $3.as.number);
            } else {
                xconf_array_append_string($1, $3.as.string);
            }
            $$ = $1;
        }
    ;

bracket_element
    : NUMBER
        {
            $$.type = XCONF_TYPE_NUMBER;
            $$.as.number = $1;
        }
    | BASENUM
        {
            /* See the matching note in a_element above. */
            $$.type = XCONF_TYPE_NUMBER;
            $$.as.number = $1.value;
            free($1.origin);
        }
    | QSTRING
        {
            $$.type = XCONF_TYPE_STRING;
            $$.as.string = $1; /* ownership moves to the array */
        }
    ;

%%

void yyerror(const char *msg)
{
    fprintf(stderr, "xconf: parse error: %s\n", msg);
}
