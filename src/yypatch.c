#include <stdio.h>
#include <stdlib.h>
#include "parser.h"


static int Last_real_nonterm; /* this is the number of the last 
                               * nonterminal to appear in the input
                               * grammar [as compared to the ones that 
                               * patch() creates]
                               */

#ifdef NEVER
  | the symbol table needs some shuffling around to make it useful 
  | in an LALR application. (it was designed to make it easy for the
  | parser). first of all, all actions must be applied on a
  | reduction rather than a shift. this means that imbedded actions
  | (like this):
  |
  |	foo : bar { act(1); } cow { act(2); };
  |
  | have to be put in their own productions (like this):
  |
  |	foo : bar {0} cow   { act(2); };
  |	{0} : /* epsilon */ { act(1); };
  |
  | where {0} is treated as a nonterminal symbol; once this is done, you
  | can print out the actions and get rid of the strings. note that,
  | since the new productions go to epsilon, this transformation does
  | not affect either the first or follow sets.
  |
  | note that you dont have to actually add any symbols to the table;
  | you can just modify the values of the action symbols to turn them
  | into nonterminals
  |
#endif

static void print_one_case(int case_val, char *action, int rhs_size, int lineno, PRODUCTION *prod)
{
  /* case_val: numeric value attached to case itself
   * action:  source code to execute in case
   * rhs_size: number of symbols on right-hand side
   * lineno: input line number (for #lines)
   * prod: pointer to right-hand side
   */

  /* print out one action as a case statement. all $-specifiers are mapped
   * to references to the value stack: $$ becomes Yy_vsp[0], $1 becomes
   * Yy_vsp[-1], etc. the rhs_size argument is used for this purpose.
   * [see do_dollar() in yydollar.c for details]
   */

  int num, i;
  char fname[40], *fp; /* place to assemble $<fname>1 */
  
  if (!Make_actions) {
    return;
  }

  output("\n  case %d: /* %s */\n\n\t", case_val, production_str(prod));

  if (!No_lines) {
    output("#line %d \"%s\"\n\t", lineno, Input_file_name);
  }

  while (*action) {
    
    if (*action != '$') {
      output("%c", *action++);
    } else {
      /* skip the attribute reference. the if statement handles $$ the
	     * else clause handles the two forms: $N and $-N, where N is a
	     * decimal number. when we hit the do_dollar call (in the output()
	     * call), "num" holds the number associated with N, or DOLLAR_DOLLAR
	     * in the case of $$.
	     */
      
      if (*++action != '<') {
        *fname = '\0';
      } else {
        ++action; /* skip the < */
        fp = fname;
        
        for (i = sizeof(fname); --i > 0 && *action && *action != '>'; ) {
          *fp++ = *action++;
        }
        
        *fp = '\0';

        if (*action == '>') {
          ++action; /* skip the > */
        }
      }

      if (*action == '$') {
        num = DOLLAR_DOLLAR;
        ++action;
      } else {
        num = atoi(action);
        if (*action == '-') {
          ++action;
        }
        while (isdigit(*action)) {
          ++action;
        }
      }

      output("%s", do_dollar(num, rhs_size, lineno, prod, fname));
    }
  }
  output("\n  break;\n");
}

/*
 * dopatch() does two things, it modifies the symbol table for use with
 * rbison and prints the action symbols. the alternative is to add another
 * field to the PRODUCTION structure and keep the action string there,
 * but this is both a needless waste of memory and an unnecessary
 * complication because the strings will be attached to a production number
 * and once we know that number, there's not point in keeping them around.
 *
 * if the action is the rightmost one on the rhs, it can be discarded
 * after printing it (because the action will be performed on reduction
 * of the indicated production), otherwise the transformation indicated
 * earlier is performed (adding a new nonterminal to the symbol table)
 * and the string is attached to the new nonterminal).
 */

static void dopatch(SYMBOL *sym)
{
  PRODUCTION *prod; /* current right-hand side of sym */
  SYMBOL **pp;      /* pointer to one symbol on rhs */
  SYMBOL *cur;      /* current element of right-hand side */
  int i;

  if (!ISNONTERM(sym) || sym->val > Last_real_nonterm) {
    /* if the current symbol isn't a nonterminal, or if it is a nonterminal that used
     * to be an action (one that we just transformed), ignore it
     */
    return;
  }

  for (prod = sym->productions; prod; prod = prod->next) {
    if (prod->rhs_len == 0) {
      continue;
    }

    pp = prod->rhs + (prod->rhs_len - 1);

    cur = *pp;
    
    if (ISACT(cur)) { /* check rightmost symbol */
      print_one_case(prod->num, cur->string, --(prod->rhs_len), cur->lineno, prod);
      delsym(Symtab, cur);
      free(cur->string);
      freesym(cur);
      *pp-- = NULL;
    }

    /* cur is no longer valid because of the --pp above 
		 * count the number of nonactions in the right-hand
		 * side	and modify imbedded actions
     */
    
    for (i = (pp - prod->rhs) + 1; --i >= 0; --pp) {
      cur = *pp;
      if (!ISACT(cur)) {
        continue;
      }

      if (Cur_nonterm >= MAXNONTERM) {
        error(FATAL, "too many nonterminals & actions (%d max)\n", MAXNONTERM);
      } else {
        /* transform the action into a nonterminal */
        Terms[cur->val = ++Cur_nonterm] = cur;
        cur->productions = (PRODUCTION *) malloc (sizeof(PRODUCTION));
        if (!cur->productions) {
          error(FATAL, "dopatch out of memory\n");
        }
        print_one_case(Num_productions, cur->string, pp - prod->rhs, cur->lineno, prod);

        /* once the case is printed, the string argument can be freed*/

        free(cur->string);
        cur->string = NULL;
        cur->productions->num = Num_productions++;
        cur->productions->lhs = cur;
        cur->productions->rhs_len = 0;
        cur->productions->rhs[0] = NULL;
        cur->productions->next = NULL;
        cur->productions->prec = 0;

        /* since the new production goes to epsilon and nothing else,
		     * FIRST(new) == {epsilon}
		     */
        cur->first = newset();
        ADD(cur->first, EPSILON);
      }
    }
  }
}

void patch()
{
  /* this subroutine does several things:
   *
   *	 * it modifies the symbol table as described in the previous text
   *	 * it prints the action subroutine and deletes the memory associated
   *	      with the actions
   */

  static char *top[] = {
	  "",
	  "yy_act(int yypnum, YYSTYPE *yyvsp) /* production number and value-stack pointer */",
	  "{",

	  " /* this subroutine holds all the actions in the original input",
	  "  * specification. it normally returns 0, but if any of your",
	  "  * actions return a non-zero number, then the parser halts",
	  "  * immediately, returning that nonzero number to the calling",
	  "  * subroutine.",
	  "  */",
	  "",
	  "  switch( yypnum )",
	  "  {",
	  NULL
  };
  
  static char *bot[] = {
	  "",
	  "  }",
	  "",
	  "  return 0;",
	  "}",
	  NULL
  };

  Last_real_nonterm = Cur_nonterm;
  
  if (Make_actions) {
    printv(Output, top);
  }

  ptab(Symtab, (ptab_t)dopatch, NULL, 0);
  
  if (Make_actions) {
    printv(Output, bot);
  }
}