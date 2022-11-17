%{
#include "yyout.h"
extern int yylval;
%}
%%
"+"	return PLUS;
"-" return SUB;
"*"	return MUL;
"/" return DIV;
"("	return LP;
")"	return RP;
[0-9]+    { yylval = atoi(yytext); return NUM; }
\n        { return EOL; }
.         { yyerror("mystery character %c\n", *yytext); }
%%