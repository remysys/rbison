
%token NUM
%%
start:
  ;
%%
int main()
{
  printf("> ");
  yyparse();
}
