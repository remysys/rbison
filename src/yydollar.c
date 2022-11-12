#include <stdio.h>
#include <string.h>
#include "parser.h"

char *do_dollar(int num, int rhs_size, int lineno, PRODUCTION *prod, char *fname)
{
  /* num: the N is $N, DOLLAR_DOLLAR for $$
   * rhs_size: number of symbols on right-hand side
   * lineno: input line number for error messages
   * prod: production
   * fname: name in $<name>N
   */

  static char buf[128];
  int i, len;

  if (num == DOLLAR_DOLLAR) { /* Do $$ */
    strcpy(buf, "Yy_val");
    
    if (*fname) {  /* $<name>N */
      sprintf(buf + 6, ".%s", fname);
    } else if (fields_active()) {
      if (*prod->lhs->field) {
        sprintf(buf + 6, ".%s", prod->lhs->field);
      } else {
        error(WARNING, "line %d: no <field> assigned to $$, ", lineno);
        error(NOHDR, "using default int field\n");
        sprintf(buf + 6, ".%s", DEF_FIELD);
      }
    }
  } else {
    if (num < 0) {
      ++num;
    }

    if ((i = rhs_size - num) < 0) {
      error(WARNING, "line %d: illegal %d in production\n", lineno, num);
    } else {
      len = sprintf(buf, "yyvsp[%d]", i);

      if (*fname) { /* $<name>N */
        sprintf(buf + len, ".%s", fname);
      } else if (fields_active()) {
        if (num <= 0) {
          error(NONFATAL, "can't use %%union field with negative");
          error(NOHDR, "attributes. use $<field>-N\n");
        } else if (*(prod->rhs)[num - 1]->field) {
          sprintf(buf + len, ".%s", (prod->rhs)[num - 1]->field);
        } else {
          error(WARNING, "line %d: no <field> assigned to $%d", lineno, num);
          error(NOHDR, "using default int field\n");
          sprintf(buf + len, ".%s", DEF_FIELD);
        }
      }
    }
  }
  return buf;
}
