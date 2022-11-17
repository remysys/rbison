%{
#include <stdio.h>
%}

%token NUM
%token EOL
%token LP RP
%left PLUS SUB
%left MUL DIV

%%

s : exprs
  ;

exprs : exprs e EOL  { printf("res = %d\n", $2); }
      |
      ;

e : e PLUS e    { $$ = $1 + $3; }
  | e SUB e   { $$ = $1 - $3; }
  | e MUL e   { $$ = $1 * $3; }
  | e DIV e   { $$ = $1 / $3; }
  | NUM       { $$ = $1; }
  | LP e RP   { $$ = $2; }
  ;
%%
int main() {
  ii_advance();
  ii_mark_start(); // skipping leading newline 
  return yyparse();
}