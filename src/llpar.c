#include <stdio.h>
#include <stdarg.h>
#include "llout.h"
#include "parser.h"

/* 
 * llpar.c:	a recursive-descent parser for a very stripped down rbison.
 * there's a rbison input specification for a table-driven parser
 * in parser.l
 */


int yynerrs; /* total error count*/
static int Lookahead; /* Lookahead token */

#define match(x) ((x) == Lookahead)

static void advance()
{
  if (Lookahead != _EOI_) {
    while ((Lookahead = yylex()) == WHITESPACE);
  }
}

static void lookfor(int first, ...) 
{
  /* 
   * read input until the current symbol is in the argument list. for example,
   * lookfor(OR, SEMI, 0) terminates when the current Lookahead symbol is an
   * OR or SEMI. searching starts with the next (not the current) symbol
   */

  for (advance(); ; advance()) {
    int *obj;
    for (obj = &first; *obj && !match(*obj); obj++) {
      continue;
    }

    if (*obj) {
      break;
    } else if (match(_EOI_)) {
      lerror(FATAL, "unexpected end of file\n");
    }
  }

}

/* the parser itself */


int yyparse(void)  /* spec : definitions body stuff */
{
  extern int yylineno;
  Lookahead = yylex(); /* get first input symbol */
  definitions();
  first_sym();
  body();
  return 0;
}

static void definitions()
{
  /* implemented at:
   * definitions : TERM_SPEC tnames definitions   1
   *        | CODE_BLOCK definitions              2
   *        | SEPARATOR                           3
   *        | _EOI_                               3
   * tnames : NAME {make_term} tnames             4
   *
   * note that relx copies the CODE_BLOCK contents to the output file
   * automatically on reading it.
   */
  while (!match(SEPARATOR) && !match(_EOI_)) { /* 3 */
    if (Lookahead == TERM_SPEC) {
      for (advance(); match(NAME); advance()) { /* 1 */
        make_term(yytext);
      }
    } else if (Lookahead == CODE_BLOCK) {
      advance(); /* the block is copied out from yylex() */
    } else {
      lerror(NONFATAL, "ignoring illegal <%s> in definitions\n", yytext);
      advance();
    }
  }

  advance(); /* advance past the %% */
}

void body()
{
  /* implemented at:
   * body : rule body       1
   *  | rule SEPARATOR      1
   *  | rule _EOI_          1
   * rule : NAME {new_nonterm} COLON right_side 2
   *      : <epsilon> 3
   */

  extern void ws(void); /* from lexical analyser */
  while (!match(SEPARATOR) && !match(_EOI_)) { /* 1 */
    if (match(NAME)) { /* 2 */
      new_nonterm(yytext, 1);
      advance();
    } else {  /* 3 */
      lerror(NONFATAL, "illegal <%s>, nonterminal expected\n", yytext);
      lookfor(SEMI, SEPARATOR, 0);
      if (match(SEMI)) {
        advance();
      }
      continue;
    }

    if (match(COLON)) {
      advance();
    } else {
      lerror(NONFATAL, "inserted missing ':'\n");
    }
    
    right_side();
  }
  ws(); /* enable white space (see parser.l) */
  if (match(SEPARATOR)) {
    yylex(); /* advance past %% */
  }
}

void right_side()
{
/* 
 * right_sides : {new_rhs} rhs OR right_sides 1
 *    | {new_rhs} rhs SEMI				            2
 */

  new_rhs();
  rhs();

  while (match(OR)) {
    advance();
    new_rhs();
    rhs();
  }

  if (match(SEMI)) {
    advance();
  } else {
    lerror(NONFATAL, "inserted missing semicolon\n");
  }
}

void rhs()
{
  /* rhs : NAME {add_to_rhs} rhs 
   *     | ACTION {add_to_rhs} rhs  
   */
  while (match(NAME) || match(ACTION)) {
    add_to_rhs(yytext, match(ACTION) ? start_action() : 0);
    advance();
  }

  if (!match(OR) && !match(SEMI)) {
    lerror(NONFATAL, "illegal <%s>, ignoring rest of production\n", yytext);
    lookfor(SEMI, SEPARATOR, OR, 0);
  }
}