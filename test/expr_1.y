/* simplest version of calculator */
%{
#include <stdio.h>
%}

%token NUM

%%
exp: NUM
  ;
%%

int main()
{
  return 0;
}