/* calculator with ast */

%{
#include <stdio.h>
#include <stdlib.h>
struct ast *newast(int type, struct ast *l, struct ast *r);
struct ast *newnum(double d);
double eval(struct ast *a);
%}

%union {
  struct ast *a;
  double d;
}

%type <a> e
%token <d> NUM
%token LP RP
%token EOL
%left PLUS SUB
%left MUL DIV
%nonassoc VERY_HIGH

%%

s : e EOL     { printf("res = %f\n", eval($1)); }
  ;

e : e PLUS e  { $$ = newast('+', $1, $3); }
  | e SUB e   { $$ = newast('-', $1, $3); }
  | e MUL e   { $$ = newast('*', $1, $3); }
  | e DIV e   { $$ = newast('/', $1, $3); }
  | LP e RP   { $$ = $2; }
  | NUM       { $$ = newnum($1); }
  | SUB e %prec VERY_HIGH { $$ = newast('M', $2, NULL); }
  ;

%%
struct ast {
  int type;
  struct ast *l;
  struct ast *r;
};

struct numval {
  int type;
  double number;
};

struct ast *newast(int type, struct ast *l, struct ast *r)
{
  struct ast *node = (struct ast *) malloc(sizeof(struct ast));
  
  if (!node) {
    yyerror("out of memory\n");
    exit(0);
  }

  node->type = type;
  node->l = l;
  node->r = r;
  return node;
}

struct ast *newnum(double d)
{
  struct numval *node = malloc(sizeof(struct numval));
  
  if (!node) {
    yyerror("out of memory\n");
    exit(0);
  }

  node->type = 'K';
  node->number = d;
  return (struct ast *)node;
}

double eval(struct ast *a)
{
  double v;

  switch(a->type) {
    case 'K': 
      v = ((struct numval *)a)->number; 
      break;
    case '+': 
      v = eval(a->l) + eval(a->r);
      break;
    case '-': 
      v = eval(a->l) - eval(a->r);
      break;
    case '*': 
      v = eval(a->l) * eval(a->r);
      break;
    case '/': 
      v = eval(a->l) / eval(a->r);
      break;
    case 'M': 
      v = -eval(a->l);
      break;
    default: 
      printf("internal error: bad node %c\n", a->type);
  }

  return v;
}

int main()
{
  ii_advance();
  ii_mark_start(); // skipping leading newline 
  yyparse();
  return 0; 
}