/* simplest version of calculator */
%{
#include <stdio.h>
%}

%token NUM
%token ADD SUB MUL DIV
%token LP RP
%token EOL

%%

start: calclist
  ;

calclist:
  | calclist exp EOL { printf("= %d\n> ", $2); }
  | calclist EOL { printf("> "); } /* blank line or a comment */
  ;

exp: factor
  | exp ADD exp { $$ = $1 + $3; }
  | exp SUB factor { $$ = $1 - $3; }
  ;

factor: term
  | factor MUL term { $$ = $1 * $3; }
  | factor DIV term { $$ = $1 / $3; }
  ;

term: NUMBER
  | LP exp RP { $$ = $2; }
  ;
%%
