#include <stdio.h>
#include <stdarg.h>
#include "llout.h"
#include "parser.h"

/* 
 * parser.c:	a recursive-descent parser for grammar file
 */

extern int yylineno;  /* created by rlex */
extern char *yytext;
extern int yylex(void);


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

void fnames()
{
  /*
   * fnames : NAME { new_nonterm(yytext, 0); } fnames
   *        | FIELD { new_field(yytext); } fnames
   *        | 
   *        ;
   */
  
  while (match(NAME) || match(FIELD)) {
    if (match(NAME)) {
      new_nonterm(yytext, 0);
      advance();
    } else {
      new_field(yytext);
      advance();
    }
  }

  if (!match(SEPARATOR) && !match(PERCENT_UNION) && !match(TYPE) && !match(TERM_SPEC) && !match(LEFT) && !match(RIGHT) && !match(NONASSOC) && !match(CODE_BLOCK)) {
    lerror(NONFATAL, "illegal <%s>, ignoring rest of definition\n", yytext);
    lookfor(SEPARATOR, PERCENT_UNION, TYPE, TERM_SPEC, LEFT, RIGHT, NONASSOC, CODE_BLOCK, 0);
  }
}

void tnames()
{
  /*
   * tnames : NAME  { make_term(yytext); } tnames
   *        | FIELD { new_field(yytext); } tnames
   *        |
   *        ;
   */

  while (match(NAME) || match(FIELD)) {
    if (match(NAME)) {
      make_term(yytext);
      advance();
    } else {
      new_field(yytext);
      advance();
    }
  }

  if (!match(SEPARATOR) && !match(PERCENT_UNION) && !match(TYPE) && !match(TERM_SPEC) && !match(LEFT) && !match(RIGHT) && !match(NONASSOC) && !match(CODE_BLOCK)) {
    lerror(NONFATAL, "illegal <%s>, ignoring rest of definition\n", yytext);
    lookfor(SEPARATOR, PERCENT_UNION, TYPE, TERM_SPEC, LEFT, RIGHT, NONASSOC, CODE_BLOCK, 0);
  }
}

void pnames()
{
  /*
   * pnames : NAME  { prec_list(yytext); } pnames
   *        | FIELD { new_field(yytext); } pnames
   *        |
   *        ;
   */

  while (match(NAME) || match(FIELD)) {
    if (match(NAME)) {
      prec_list(yytext);
      advance();
    } else {
      new_field(yytext);
      advance();
    }
  }

  if (!match(SEPARATOR) && !match(PERCENT_UNION) && !match(TYPE) && !match(TERM_SPEC) && !match(LEFT) && !match(RIGHT) && !match(NONASSOC) && !match(CODE_BLOCK)) {
    lerror(NONFATAL, "illegal <%s>, ignoring rest of definition\n", yytext);
    lookfor(SEPARATOR, PERCENT_UNION, TYPE, TERM_SPEC, LEFT, RIGHT, NONASSOC, CODE_BLOCK, 0);
  }
}

static void definitions()
{
  /*
   * defs : PERCENT_UNION ACTION          { union_def(yytext); } defs
   *      | TYPE                       fnames { new_field(""); } defs
   *      | TERM_SPEC  { new_lev(0); } tnames { new_field(""); } defs
   *      | LEFT     { new_lev('l'); } pnames { new_field(""); } defs
   *      | RIGHT    { new_lev('r'); } pnames { new_field(""); } defs
   *      | NONASSOC { new_lev('n'); } pnames { new_field(""); } defs
   *      | CODE_BLOCK defs
   *      | 
   *      ;
   */

  while (!match(SEPARATOR) && !match(_EOI_)) {
    if (Lookahead == PERCENT_UNION) {
      advance();
      if (match(ACTION)) {
        union_def(yytext);
        advance();
      } else {
        lerror(NONFATAL, "ignoring illegal <%s> in definitions\n", yytext);
        advance();
      }
    } else if (Lookahead == TYPE) {
      advance();
      fnames();
    } else if (Lookahead == TERM_SPEC) {
      new_lev(0);
      advance();
      tnames();
    } else if (Lookahead == LEFT) {
      new_lev('l');
      advance();
      pnames();
    } else if (Lookahead == RIGHT) {
      new_lev('r');
      advance();
      pnames();
    } else if (Lookahead == NONASSOC) {
      new_lev('n');
      advance();
      pnames();
    } else if (Lookahead == CODE_BLOCK) {
      advance(); /* the block is copied out from yylex() */
    } else {
      lerror(NONFATAL, "ignoring illegal <%s> in definitions\n", yytext);
      advance();
    }
  }

  advance(); /* advance past the %% */
}

void rhs()
{
  /*
   * rhs : NAME  { add_to_rhs(yytext, 0); } rhs
   *     | FIELD { add_to_rhs(yytext, 0); } rhs
   *     | ACTION { add_to_rhs(yytext, start_action()); } rhs
   *     | PREC NAME { prec(yytext); } rhs
   *     |
   *     ;
   */
  
  while (match(NAME) || match(FIELD) || match (ACTION) || match(PREC)) {

    if (match(NAME) || match(FIELD) ) {
      add_to_rhs(yytext, 0);
      advance();
    } else if (match(ACTION)) {
      add_to_rhs(yytext, start_action());
      advance();
    } else {
      advance();
      if (match(NAME)) {
       prec(yytext);
      } else {
        lerror(NONFATAL, "illegal <%s>, ignoring rest of production\n", yytext);
        lookfor(SEMI, SEPARATOR, OR, 0);
      }
    }
  }

  if (!match(OR) && !match(SEMI)) {
    lerror(NONFATAL, "illegal <%s>, ignoring rest of production\n", yytext);
    lookfor(SEMI, SEPARATOR, OR, 0);
  }
}

void right_side()
{
/*
 * right_sides : { new_rhs(); } rhs end_rhs
 *             ;
 * end_rhs : OR right_sides
 *         | SEMI
 *         ;
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

void body()
{
  /*
   * rules : rule rules
   *       |
   *       ;
   * 
   * rule  : NAME { new_nonterm(yytext, 1); } COLON right_sides
   *       ;
   */
  
  extern void ws(void); /* from lexical analyser */

  while (!match(SEPARATOR) && !match(_EOI_)) {
    if (match(NAME)) {
      new_nonterm(yytext, 1);
      advance();
    } else {
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

/* the parser itself */

int yyparse(void)
{
  /*
   * spec : defs SEPARATOR { first_sym(); } rules end
   *      ;
   * end : { ws(); } SEPARATOR
   *     |
   */
  
  extern int yylineno;
  Lookahead = yylex(); /* get first input symbol */
  definitions();
  first_sym();
  body();
  return 0;
}