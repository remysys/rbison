#include <stdio.h>
#include <set.h>
#include <hash.h>
#include "parser.h"

/* 
 * first.c compute FIRST sets for all productions in a symbol table.
 */

static int diff;

void first_closure(SYMBOL *lhs)
{
  /*
   * lhs: current left-hand side 
   * called for every element in the FIRST sets. adds elements to the first
   * sets. The following rules are used:
   *
   * 1) given lhs->...Y... where Y is a terminal symbol preceded by any number
   *    (including 0) of nullable nonterminal symbols or actions, add Y to
   *    FIRST(x)
   *
   * 2) given lhs->...y... where y is a nonterminal symbol preceded by any
   *    number (including 0) of nullable nonterminal symbols or actions, add
   *    FIRST(y) to FIRST(lhs)
   */

  PRODUCTION *prod;       /* pointer to one production side */
  SYMBOL **y;             /* pointer to one element of production */
  static SET *set = NULL; 
  int i;

  if (!ISNONTERM(lhs)) { /* ignore entries for terminal symbols */
    return;
  }

  if (!set) {
    set = newset();
  }

  ASSIGN(set, lhs->first);
  
  for (prod = lhs->productions; prod; prod = prod->next) {
    if (prod->non_acts == 0) { /* no non-action symbols */
      ADD(set, EPSILON);       /* add epsilon to first set */
      continue;
    }

    for (y = prod->rhs, i = prod->rhs_len; --i >= 0; y++) {
      if (ISACT(*y)) { /* pretend acts don't exist */
        continue;
      }

      if (ISTERM(*y)) {
        ADD(set, (*y)->val);
      } else {      /* it's a nonterminal */
        UNION(set, (*y)->first);
      }

      if (!NULLABLE(*y)) {      /* it's not a nullable nonterminal  */
        break;
      }
    }
  }

  if (!IS_EQUIVALENT(set, lhs->first)) {
    ASSIGN(lhs->first, set);
    diff = 1;
  }
}

void first()
{
  /* construct FIRST sets for all nonterminal symbols in the symbol table */
  do {
    diff = 0;
    ptab(Symtab, (ptab_t)first_closure, NULL, 0);
  } while (diff);
}

int first_rhs(SET *dest, SYMBOL **rhs, int len)
{
  /* 
   * dest: target set
   * rhs: a right-hand side 
   * len: # of objects in rhs 
   * fill the destination set with FIRST(rhs) where rhs is the right-hand side
   * of a production represented as an array of pointers to symbol-table
   * elements. return 1 if the entire right-hand side is nullable, otherwise
   * return 0
   */

  if (len <= 0) {
    ADD(dest, EPSILON);
    return 1;
  }

  for (; --len >= 0; ++rhs) {
    if(ISACT(*rhs)) {
      continue;
    }

    if (ISTERM(*rhs)) {
      ADD(dest, (*rhs)->val);
    } else {
      UNION(dest, (*rhs)->first);
    }
  
    if (!NULLABLE(*rhs)) {
      break;
    }
  }

  return (len < 0);
}
