#include <stdio.h>
#include "parser.h"

void tables()
{
  make_yy_stok();      /* in stok.c */ 
  make_token_file();   /* in stok.c */
  make_parse_tables(); /* in yystate.c */   
}