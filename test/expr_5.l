%{
#include <stdlib.h>
#include "y.tab.h"

extern union {        /* this definition must duplicate the %union in expr_5.y */
  int yy_def; /* default field, used when no %type found */
  struct ast *a;
  double d;
} yylval;            /* declared by rbison in y.tab.c */
%}

%%
"+" return PLUS;
"-" return SUB;
"*" return MUL;
"/" return DIV;
"(" return LP;
")" return RP;

[0-9]+"."?[0-9]* { yylval.d = atof(yytext); return NUM; }
\n      { return EOL; }
[\s\t]   { /* ignore white space */ }
.       { yyerror("mystery character %c\n", *yytext); }

%%