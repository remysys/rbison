%{
#include "yyout.h"
%}
%%
"+" return ADD;
"-" return SUB;
"*" return MUL;
"/" return DIV;
"(" return LP;
")" return RP;
[0-9]+ return NUM;
\n { return EOL; }
[\x00-\s] { /* ignore white space */ }
.	{ yyerror("mystery character %c\n", *yytext); }
%%