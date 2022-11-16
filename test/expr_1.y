/* simplest version of calculator */
%{
#include <stdio.h>
%}

%token NUM
%token AD SUB MUL DIV
%token EOL

%%

start: calclist
  ;

calclist:
  | calclist exp EOL { printf("= %d\n> ", $2); }
  | calclist EOL { printf("> "); } /* blank line or a comment */
  ;

exp: factor
  | factor AD exp  { $$ = $1 + $3; }
  | factor SUB exp { $$ = $1 - $3; }
  ;

factor: term
  | term MUL factor { $$ = $1 * $3; }
  | term DIV factor { $$ = $1 / $3; }
  ;

term: NUM
  ;
%%

