#ifndef _PARSER_H
#define _PARSER_H

#include "set.h"
#include "hash.h"

/*
 * various error exit stat. note that other errors found while parsing cause
 * rbison to exit with a status equal to the number of errors (or zero if
 * there are no errors)
 */

#define EXIT_ILLEGAL_ARG 255 /* illegal command-line switch */
#define EXIT_TOO_MANY    254 /* too many command-line args */
#define EXIT_NO_DRIVER   253 /* can't find rbison.par */
#define EXIT_OTHER       252 /* other error (syntax error, etc) */
#define EXIT_USR_ABRT    251 /* ctrl-break */


#define MAXNAME     32  /* maximum length of a terminal or nonterminal name */
#define MAXPROD     512 /* maximum number of productions */
#define MINTERM     1   /* token values assigned to terminals start here */
#define MINNONTERM  256 /* nonterminals start here */
#define MINACT      512 /* acts start here */

/* 
 * maximum numeric values used for terminals and nonterminals (MAXTERM and MAXNONTERM), as
 * well as the maximum number of terminals and nonterminals (NUMTERMS and NUMNONTERMS).
 * finally, USED_TERMS and USED_NONTERMS are the number of these actually in use (i.e.
 * were declared in the input file).
 */

#define MAXTERM     (MINNONTERM - 2)
#define MAXNONTERM  (MINACT - 1)

#define NUMTERMS    ((MAXTERM - MINTERM) + 1)
#define NUMNONTERMS ((MAXNONTERM - MINNONTERM) + 1)

#define USED_TERMS    ((Cur_term - MINTERM) + 1)
#define USED_NONTERMS ((Cur_nonterm - MINNONTERM) + 1)

/* 
 * these macros evaluate to true if x represents a terminal (ISTERM), nonterminal (ISNONTERM)
 * or action (ISACT)
 */
#define ISTERM(x)     ((x) && (MINTERM <= (x)->val && (x)->val <= MAXTERM))
#define ISNONTERM(x)  ((x) && (MINNONTERM <= (x)->val && (x)->val <= MAXNONTERM))
#define ISACT(x)      ((x) && (MINACT <= (x)->val))

/* epsilon's value is one more than the largest terminal actually used. we can get away with
 * this only because EPSILON is not used until after all the terminals have been entered into 
 * the symbol table.
 */
#define EPSILON (Cur_term + 1)


/* the following macros are used to adjust the nonterminal values so that the smallest
 * nonterminal is zero. (you need to do this when you output the tables). ADJ_VAL does
 * the adjustment, UNADJ_VAL translates the adjust value back to the original value.
 */

#define ADJ_VAL(x)    ((x) - MINNONTERM)
#define UNADJ_VAL(x)  ((x) + MINNONTERM)

#define NONFATAL 0  /* values to pass to error() and lerror() */
#define FATAL    1  /* defined in main.c */
#define WARNING  2  
#define NOHDR    3
#define DOLLAR_DOLLAR ((unsigned)~0 >> 1) /* passed to do_dollar() to indicate that $$ is to be processed */

#define TOKEN_FILE "yyout.h"    /* output file for token #defines */
#define PARSE_FILE "yyout.c"    /* output file for parser */
#define ACT_FILE   "yyact.c"    /* used for output if -a specified */
#define TAB_FILE   "yyoutab.c"  /* output file for parser tables (-T) */
#define SYM_FILE   "yyout.sym"  /* output file for symbot table */
#define DOC_FILE   "yyout.doc"  /* LALR(1) state machine description */
#define PAR_TEMPL  "rbison.par" /* template file for PARSE_FILE */
#define ACT_TEMPL  "rbison-act.par" /* template file for ACT_FILE */
#define PROG_NAME  "rbison"

/* 
 * the following are used to define types of the OUTPUT transition tables. the
 * ifndef takes care of compiling the rbison output file that will be used to
 * recreate the rbison input file. we must let the rbison-generated definitions
 * take precedence over the the default ones in parser.h in this case
 */

#ifndef CREATING_RBISON_PARSER
typedef int YY_TTYPE;
#endif

/* SYMBOL structure (used for symbol table). note that the name itself */
/* is kept by the symbol-table entry maintained by the hash function.  */

#define NAME_MAX 32   /* max name length + 1 */

typedef struct _symbol_
{
  char name[NAME_MAX];        /* symbol name. must be first */
  char field[NAME_MAX];       /* %type <field> */
  unsigned int val;           /* numeric value of symbol */
  unsigned int used;          /* symbol used on an rhs */
  unsigned int set;           /* symbol defined */
  unsigned int lineno;        /* input line num of string */
  char *string;               /* code for actions */
  struct _prod_ *productions; /* right-hand sides if nonterm */
  SET *first;                 /* FIRST set*/
} SYMBOL;

#define NULLABLE(sym) (ISNONTERM(sym) && MEMBER((sym)->first, EPSILON))

/* PRODUCTION structure. represents right-hand sides */
#define MAXRHS  31   /* maximum number of objects on a right-hand side */
#define RHSBITS 5    /* number of bits required to hold MAXRHS */

typedef struct _prod_
{
  unsigned int num;        /* production number */
  SYMBOL *rhs[MAXRHS + 1]; /* tokenized right-hand side */
  SYMBOL *lhs;             /* left-hand side */
  unsigned char rhs_len;   /* # of elements in rhs[] array */
  unsigned char non_acts;  /* that are not actions */
  struct _prod_ *next;     /* pointer to next production for this left-hand side */ 
  int prec;                /* relative precedence */
} PRODUCTION;

typedef struct _prectab_
{
  unsigned char level; /* relative precedence 0=none 1=lowest */
  unsigned char assoc; /* associativity 'l'=left 'r'=right '\0'=none */
} PREC_TAB;

#define DEF_FIELD "yy_def"  /* field name for default field in a %union */

#ifdef ALLOCATE
#define CLASS 
#define I(x) x
#else
#define CLASS extern
#define I(x)
#endif

/* the following are set in main.c, mostly by command-line switches */

CLASS char *Input_file_name I( = "console" ); /* input file name */
CLASS int Make_actions      I( = 1 );         /* == 0 if -p on command line */
CLASS int Make_parser       I( = 1 );         /* == 0 if -a on command line */
CLASS int Make_yyoutab      I( = 0 );         /* == 1 if -T on command line */
CLASS int No_lines          I( = 0 );         /* suppress #lines in output */ 
CLASS int No_warnings       I( = 0 );         /* suppress warnings if true */
CLASS FILE *Output          I( = stdout );    /* Output stream */
CLASS int Public            I( = 0 );         /* make static symbols public */
CLASS int Symbols           I( = 0 );         /* generate symbol table */
CLASS int Threshold         I( = 4 );         /* compression threshold */
CLASS int Uncompressed      I( = 0 );         /* don't compress tables */
CLASS int Use_stdout        I( = 0 );         /* -t specified on command line */
CLASS int Verbose           I( = 0 );         /* Verbose-mode output (1 for -v and 2 for -V)*/

/* this array is indexed by terminal or nonterminal value and evaluates to a
 * pointer to the equivalent symbol-table entry.
 */

CLASS SYMBOL *Terms[MINACT]; 

/* holds relative precedence and associativity information
 * for both terminals and nonterminals
 */

CLASS PREC_TAB Precedence[MINNONTERM]; 

CLASS char *Template I( = PAR_TEMPL ); /* template file for the parser; can be modified in main.c */
CLASS HASH_TAB *Symtab;                /* the symbol table itself initialized in yyact.c */
CLASS SYMBOL *Goal_symbol I( = NULL ); /* pointer to symbol-table entry for the start(goal) symbol */
 
/* the following are used by the acts in yyact.c */

CLASS int Cur_term        I( = MINTERM - 1);     /* current terminal */
CLASS int Cur_nonterm     I( = MINNONTERM - 1);  /* current nonterminal */
CLASS int Cur_act         I( = MINACT - 1);      /* current action */
CLASS int Num_productions I( = 0 );              /* number of productions */  

#undef CLASS
#undef I

#define outc(c) putc(c, Output); /* character routine to complement output() in main.c */

/* external variables and subroutines */

extern int  yynerrs;

/* yynerrs is the total error count. it is created automatically by rbison, so there is one definition
 * of it in the rbison output file. there's also a definition of in in the recursive-descent parser,
 * llpar.c.
 */

char *production_str(struct _prod_ *prod);            /* acts.c */
int problems(void);                                   /* acts.c */
SYMBOL *make_term(char *name);                        /* acts.c */
SYMBOL *new_nonterm(char *name, int is_lhs);          /* acts.c */
void add_to_rhs(char *object, int is_an_action);      /* acts.c */
int fields_active(void);      	                      /* acts.c */
void first_sym(void);                                 /* acts.c */
void init_acts(void );                                /* acts.c */
void new_field(char *field_name);                     /* acts.c */
void new_lev(int how);                                /* acts.c */
void new_rhs(void );                                  /* acts.c */
void pact(SYMBOL *sym, FILE *stream);                 /* acts.c */
void pnonterm(SYMBOL *sym, FILE *stream);             /* acts.c */
void prec(char *name);                                /* acts.c */
void prec_list(char *name);                           /* acts.c */
void print_symbols(FILE *stream);                     /* acts.c */
void print_tok(FILE *stream, char *format, int arg);  /* acts.c */
void pterm(SYMBOL *sym, FILE *stream);                /* acts.c */
void union_def(char *action);                         /* acts.c */
#endif
 