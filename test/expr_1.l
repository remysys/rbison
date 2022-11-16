%{
#include "yyout.h"
extern int yylval;
%}

%%
"+" return AD;
"-" return SUB;
"*" return MUL;
"/" return DIV;

[0-9]+  yylval = atoi(yytext);
        return NUM;

\n { return EOL; }

[\x00-\s] { /* ignore white space */ }

.	{ yyerror("mystery character %c\n", *yytext); }
%%