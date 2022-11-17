%{
#include <stdio.h>
%}

%token NUM
%token LP RP
%token EOL
%left MUL DIV
%left PLUS SUB

%%
s : e EOL { printf("res = %d\n", $1); };
e : e PLUS e  { $$ = $1 + $3; }
  | e SUB e   { $$ = $1 - $3; }
  | e MUL e   { $$ = $1 * $3; }
  | e DIV e   { $$ = $1 / $3; }
  | LP e RP   { $$ = $2; }
  | NUM       { $$ = $1; }
	;
%%
int main() {
  ii_advance();
  ii_mark_start(); // skipping leading newline 
  return yyparse();
}