/* simplest version of calculator */
%{
#include <stdio.h>
%}

%token NUM
%token PLUS SUB MUL DIV
%token EOL

%%

start : calclist
      ;

calclist :
         | calclist exp EOL { printf("= %d\n> ", $2); }
         | calclist EOL { printf("> "); } /* blank line or a comment */
         ;

exp : factor
    | exp PLUS factor  { $$ = $1 + $3; }
    | exp SUB factor { $$ = $1 - $3; }
    ;

factor : term
       | factor MUL term { $$ = $1 * $3; }
       | factor DIV term { $$ = $1 / $3; }
       ;

term : NUM
     ;
%%
int main(int argc, char *argv[])
{
  ii_advance();
  ii_mark_start(); // skipping leading newline 
  yyparse();
  return 0; 
}

