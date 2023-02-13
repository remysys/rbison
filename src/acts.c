#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <compiler.h>
#include <set.h>
#include <hash.h>
#include <ctype.h>


#include <stack.h>  /* stack-manipulation macros */
#include <string.h>
#undef  stack_cls   /* make all stacks static */
#define stack_cls static

#include "parser.h"
#include "llout.h"

/* acts.c action routines used by rbison. these build up the symbol 
 * table from the input specification.
 */

/* prototypes for public functions are in parser.h */

extern int yylineno;  /* input line number --created by lex */

static char Field_name[NAME_MAX];   /* Field name specified in <name> */
static int Goal_symbol_is_next = 0; /* if true, the next nonterminal is the goal symbol */

static int Associativity;       /* current associativity direction */
static int Prec_lev = 0;        /* precedence level. incremented after finding %left, etc. */
static int Fields_active = 0;   /* fields are used in the input. (if they're not, then automatic */
                                /* field-name generation, as per %union, is not activated */

typedef struct _cur_sym_
{
  char lhs_name[NAME_MAX];    /* name associated with left-hand side */
  SYMBOL *lhs;                /* pointer to symbol-table entry for the current left-hand side */
  PRODUCTION *rhs;            /* pointer to current production */
} CUR_SYM;

static CUR_SYM cur_sym;
static CUR_SYM *p_cur_sym = &cur_sym;

/* support routines for actions */


void print_tok(FILE *stream, char *format, int arg) 
{
  /* print one nonterminal symbol to the specified stream */
  if (arg == -1) {
    fprintf(stream, "null ");
  } else if (arg == -2) {
    fprintf(stream, "empty ");
  } else if (arg == _EOI_) {
    fprintf(stream, "$ ");
  } else if (arg == EPSILON) {
    fprintf(stream, "<epsilon> ");
  } else {
    fprintf(stream, "%s ", Terms[arg]->name);
  }
}

/* 
 * the following three routines print the symbol table. Pterm(), pact(), and
 * pnonterm() are all called indirectly through ptab(), called in
 * print_symbols(). they print the terminal, action, and nonterminal symbols
 * from the symbol table, respectively.
 */

void pterm(SYMBOL *sym, FILE *stream)
{
  int i;
  if (!ISTERM(sym)) {
    return;
  }

  fprintf(stream, "%-16.16s  %3d    %2d   %c    <%s>\n",
        sym->name,
        sym->val,
        Precedence[sym->val].level,
        (i = Precedence[sym->val].assoc) ? i : '-',
        sym->field);
}

void pact(SYMBOL *sym, FILE *stream)
{
  if (!ISACT(sym)) {
    return;
  }

  fprintf(stream, "%-5s %3d", sym->name, sym->val);
  fprintf(stream, " line %-3d: ", sym->lineno);
  fputstr(sym->string, 55, stream);
  fprintf(stream, "\n");
}

char *production_str(PRODUCTION *prod) 
{
  /* return a string representing the production */
  int i, nchars, avail;
  static char buf[80];
  char *p;

  nchars = sprintf(buf, "%s ->", prod->lhs->name);
  p = buf + nchars;
  avail = sizeof(buf) - nchars;

  if (!prod->rhs_len) {
    sprintf(p, " (epsilon)");
  } else {
    for (i = 0; i < prod->rhs_len && avail > 0; i++) {
      nchars = sprintf(p, " %0.*s", avail - 1, prod->rhs[i]->name);
      avail -= nchars;
      p += nchars;
    }
  }
  return buf;
}


void pnonterm(SYMBOL *sym, FILE *stream)
{
  PRODUCTION *p;
  int chars_printed;
  stack_dcl(pstack, PRODUCTION *, MAXPROD);
  
  if (!ISNONTERM(sym)) {
    return;
  }

  fprintf(stream, "%s (%3d)  %s", sym->name, sym->val, 
      sym == Goal_symbol ? "(goal symbol)" : "");

  fprintf(stream, " <%s>\n", sym->field);

  if (Symbols > 0) {
    /* print first sets only if you want really verbose output */
    fprintf(stream, "  FIRST: ");
    pset(sym->first, (pset_t)print_tok, stream);
    fprintf(stream, "\n");
  }

  /* productions are put into the SYMBOL in reverse order because it's easier
   * to tack them on to the beginning of the linked list. it's better to print
   * them in forward order, however, to make the symbol table more readable.
   * solve this problem by stacking all the productions and then popping
   * elements to print them. since the pstack has MAXPROD elements, it's not
   * necessary to test for stack overflow on a push
   */
  
  for (p = sym->productions; p; p = p->next) {
    push(pstack, p);
  }

  while (!stack_empty(pstack)) {
    p = pop(pstack);
    chars_printed = fprintf(stream, "   %3d: %s", p->num, production_str(p));

    if (p->prec) {
      for ( ; chars_printed <= 60; ++chars_printed) {
        putc('.', stream);
      }

      fprintf(stream, "PREC %d", p->prec);
    }
    putc('\n', stream);
  }

  fprintf(stream, "\n");
}


void print_symbols(FILE *stream)
{
  /* print out the symbol table. nonterminal symbols come first.
   * symbols other than production numbers can be entered symbolically. 
   * ptab returns 0 if it can't print the symbols sorted (because there's no memory. 
   * if this is the case, try again but print the table unsorted).
   */ 
  
  putc('\n', stream);
  fprintf(stream, "---------------- Symbol table ------------------\n");
  fprintf(stream, "\nNONTERMINAL SYMBOLS:\n\n");
  fprintf(stream, "name             value  prec  assoc   field\n");


  if (ptab(Symtab, (ptab_t)pnonterm, stream, 1) == 0) {
    ptab(Symtab, (ptab_t)pnonterm, stream, 0);
  }
}

/*
 * problems() and find_problems work together to find unused symbols and
 * symbols that are used but not defined.
 */

void find_problems(SYMBOL *sym) 
{
  if (!sym->used && sym != Goal_symbol) {
    error(WARNING, "<%s> not used (define on line %d)\n", sym->name, sym->set);
  }

  if (!sym->set && !ISACT(sym)) {
    error(NONFATAL, "<%s> not defined (used on line %d)\n", sym->name, sym->used);
  }
}

int problems()
{
  /* find and print an error message, for all symbols that are used but not
   * defined, and for all symbols that are defined but not used. return the
   * number of errors after checking
   */
  ptab(Symtab, (ptab_t)find_problems, NULL, 0);
  return yynerrs;
}


void init_acts()
{
  /* various initializations that can't be done at compile time. call this
   * routine before starting up the parser. the hash-table size (157) is
   * an arbitrary prime number, roughly the number symbols expected in the
   * table. note that using hash_pjw knocks about 25% off the execution
   * time as compared to hash_add
   */
   
  static SYMBOL bogus_symbol;
  strcpy(bogus_symbol.name, "end of input");
  Terms[0] = &bogus_symbol;
  
  Symtab = maketab(157, hash_pjw, strcmp);
}

static int c_identifier(char *name) /* return true only if name is a legitimate C identifier */
{
  if (isdigit(*name)) {
    return 0;
  }

  for (; *name; ++name) {
    if (!(isalnum(*name) || *name == '_')) {
      return 0;
    }
  }

  return 1;
}

SYMBOL *make_term(char *name)   /* make a terminal symbol */
{
  SYMBOL *p;

  if (!c_identifier(name)) {
    lerror(NONFATAL, "token names must be legitimate C identifiers\n");
    p = NULL;
  } else if (p = (SYMBOL *) findsym(Symtab, name)) {
    lerror(WARNING, "terminal symbol <%s> already declared\n", name);
  } else {
    if (Cur_term >= MAXTERM) {
      lerror(FATAL, "too many terminal symbols (%d max)\n", MAXTERM);
    }
    p = (SYMBOL *) newsym(sizeof(SYMBOL));
    strncpy(p->name, name, NAME_MAX);
    strncpy(p->field, Field_name, NAME_MAX);
    addsym(Symtab, p);
    p->val = ++Cur_term;
    p->set = yylineno;
    Terms[Cur_term] = p;
  }

  return p;
}

void first_sym()
{
  /* this routine is called just before the first rule following the %%
   * it's used to point out the goal symbol
   */
  
  Goal_symbol_is_next = 1;
}

SYMBOL *new_nonterm(char *name, int is_lhs)
{
/* create, and initialize, a new nonterminal. is_lhs is used to
 * differentiate between implicit and explicit declarations. it's 0 if the
 * nonterminal is added because it was found on a right-hand side. it's 1 if
 * the nonterminal is on a left-hand side.
 *
 * return a pointer to the new symbol or NULL if an attempt is made to use a
 * terminal symbol on a left-hand side.
 */
  SYMBOL *p;

  if (p = (SYMBOL *) findsym(Symtab, name)) {
    if (!ISNONTERM(p)) {
      lerror(NONFATAL, "symbol on left-hand side must be nonterminal\n");
      p = NULL;
    }
  } else if (Cur_nonterm >= MAXNONTERM) {
    lerror(FATAL, "too many nonterminal symbols (%d max)\n", MAXNONTERM);
  } else { /* add new nonterminal to symbol table */
    p = (SYMBOL *) newsym(sizeof(SYMBOL));
    strncpy(p->name, name, NAME_MAX);
    strncpy(p->field, Field_name, NAME_MAX);
    p->val = ++Cur_nonterm;
    Terms[Cur_nonterm] = p;
    addsym(Symtab, p);
  }

  if (p) { /* (re)initialize new nonterminal */
    if (Goal_symbol_is_next) {
      Goal_symbol = p;
      Goal_symbol_is_next = 0;
    }

    if (!p->first) {
      p->first = newset();
    }
    p->lineno = yylineno;

    if (is_lhs) {
      strncpy(p_cur_sym->lhs_name, name, NAME_MAX);
      p_cur_sym->lhs = p;
      p_cur_sym->rhs = NULL;
      p_cur_sym->lhs->set = yylineno;
    }
  }

  return p;
}


void new_rhs()
{
  /* get a new PRODUCTION and link it to the head of the production chain.
   * of the current nonterminal. note that the start production MUST be
   * production 0. as a consequence, the first rhs associated with the first
   * nonterminal MUST be the start production. Num_productions is initialized
   * to 0 when it's declared.
   */
  PRODUCTION *p;
  if (!(p = (PRODUCTION *) calloc(1, sizeof(PRODUCTION)))) {
    lerror(FATAL, "no memory for new right-hand side\n");
  }

  p->next = p_cur_sym->lhs->productions;
  p_cur_sym->lhs->productions = p;

  if ((p->num = Num_productions++) >= MAXPROD) {
    lerror(FATAL, "too many productions (%d max)\n", MAXPROD);
  }

  p->lhs = p_cur_sym->lhs;
  p_cur_sym->rhs = p;
}


/*
 * is_an_action: 0 of not an action, line number otherwise
 */

void add_to_rhs(char *object, int is_an_action) 
{
  SYMBOL *p;
  int i;
  char buf[32];

  /* add a new element to the RHS currently nonterminal symbol. first deal with
   * forward references. if the item isn't in the table, add it. note that,
   * since terminal symbols must be declared with a %term directive, forward
   * references always refer to nonterminals or action items. when we exit the
   * if statement, p points at the symbol table entry for the current object.
   */
  
  if (!(p = (SYMBOL *) findsym(Symtab, object))) { /* not in tab yet */
    if (!is_an_action) {
      if (!(p = new_nonterm(object, 0))) {
        /* won't get here unless p is a terminal symbol */
        lerror(FATAL, "(internal) unexpected terminal symbol\n");
        return;
      }
    } else {
      /* add an action. all actions are named "{DDD}" where DDD is the
       * action number. the curly brace in the name guarantees that this
       * name won't conflict with a normal name. i am assuming that calloc
       * is used to allocate memory for the new node (ie. that it's
       * initialized to zeros).
       */
      sprintf(buf, "{%d}", ++Cur_act - MINACT);
      p = (SYMBOL *) newsym(sizeof(SYMBOL));
      strncpy(p->name, buf, NAME_MAX);
      addsym(Symtab, p);

      p->val = Cur_act;
      p->lineno = is_an_action;

      if (!(p->string = strdup(object))) {
        lerror(FATAL, "no memory to save action\n");
      }
    }
  }

  p->used = yylineno;
  if ((i = p_cur_sym->rhs->rhs_len++) >= MAXRHS) {
    lerror(NONFATAL, "right-hand side too long (%d max)\n", MAXRHS);
  } else {
    if (ISTERM(p)) {
      p_cur_sym->rhs->prec = Precedence[p->val].level;
    }

    p_cur_sym->rhs->rhs[i] = p;
    p_cur_sym->rhs->rhs[i + 1] = NULL; /* null terminal the array */

    if (!ISACT(p)) {
      ++(p_cur_sym->rhs->non_acts);
    }
  }
}

void new_lev(int how) 
{
  /* increment the current precedence level and modify "Associativity"
   * to remember if we're going left, right, or neither.
   */
  
  if (Associativity = how) { /* 'l', 'r', 'n', (0 if unspecified) */
    ++Prec_lev;
  }
}


void prec_list(char *name)
{
  /* add current name (in yytext) to the precision list. "Associativity" is
   * set to 'l', 'r', or 'n', depending on whether we're doing a %left,
   * %right, or %nonassoc. also make a nonterminal if it doesn't exist
   * already.
   */
  SYMBOL *sym;
  if (!(sym = (SYMBOL *) findsym(Symtab, name))) {
    sym = make_term(name);
  }
  
  if (!ISTERM(sym)) {
    lerror(NONFATAL, "%%left or %%right, %s must be a token\n", name);
  } else {
    Precedence[sym->val].level = Prec_lev;
    Precedence[sym->val].assoc = Associativity;
  }
}

void prec(char *name)
{
  /* change the precedence level for the current right-hand side, using
  * (1) an explicit number if one is specified, or (2) an element from the
  * Precedence[] table otherwise.
  */

  SYMBOL *sym;

  if (isdigit(*name)) {
    p_cur_sym->rhs->prec = atoi(name); /* (1) */
  } else {
    if (!(sym = (SYMBOL *) findsym(Symtab, name))) {
      lerror(NONFATAL, "%s (used in %%prec) undefined\n", name);
    } else if (!ISTERM(sym)){
      lerror(NONFATAL, "%s (used in %%prec) must be terminal symbol\n", name);
    } else {
      p_cur_sym->rhs->prec = Precedence[sym->val].level;
    }
  }
}


void union_def(char *action)
{
  /* create a YYSTYPE definition for the union, using the fields specified
   * in the %union directive, and also appending a default integer-sized
   * field for those situation where no field is attached to the current
   * symbol.
   */
  
  while (*action && *action != '{') { /* get rid of everything up to the open brace */
    ++action;
  }

  if (*action) {
    ++action;
  }

  
  output("typedef union\n");
  output("{\n");
  output("  int %s; /* default field, used when no %%type found */", DEF_FIELD);
  output("%s\n", action);
  output("yystype;\n\n");
  output("#define YYSTYPE yystype\n");
  Fields_active = 1;
}

int fields_active(void)
{
  return Fields_active; /* previous %union was specified */
}

void new_field(char *field_name)
{
  /*change the name of the current <field> */
  char *p;
  if (!*field_name) {
    *Field_name = '\0';
  } else {
    if (p = strchr(++field_name, '>')) {
      *p = '\0';
    }

    strncpy(Field_name, field_name, sizeof(Field_name));
  }
}