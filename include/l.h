#ifndef _L_H
#define _L_H

/* ---------------- input.c ---------------- */

void ii_io(int (*open_funct)(char *, int), int (*close_funct)(int), int (*read_funct)(int, void *, unsigned int)); 
int ii_newfile (char *name);
char *ii_text();
int ii_length();
int ii_lineno();


char *ii_ptext();
int ii_plength();
int ii_plineno();

char *ii_mark_start();
char *ii_mark_end();
char *ii_move_start();
char *ii_to_mark();
char *ii_mark_prev();

int ii_advance();
int ii_flush(int force);
int ii_fillbuf(char *starting_at);
int ii_look(int n);
int ii_pushback(int n);

void ii_term();
void ii_unterm();
int ii_input();
void ii_unput(int c);

int ii_lookahead(int n);
int ii_flushbuf();

/* ---------------- yywrap.c ---------------- */
int yywrap( );

/* ---------------- yyinitlex.c ---------------- */
void yy_init_lex();

/* ---------------- ferr.c ---------------- */
int ferr(char *fmt, ...);

/* ---------------- yyinitox.c -------------*/
void yy_init_rbison(void *tos);

extern FILE  *yycodeout;    /* output stream (code) */
extern FILE  *yybssout;     /* output stream (bss ) */
extern FILE  *yydataout;    /* output stream (data) */

extern char *yytext;        /* declared by lex in lex.yy.c */
extern int  yylineno;
extern int  yyleng;

void  yycode      (char *fmt, ...);
void  yydata      (char *fmt, ...); 
void  yybss      (char *fmt, ...);
void  yyerror     (char *fmt, ...);
void  yycomment   (char *fmt, ...);
int yyparse (void);
int yylex (void);

extern char *yytext;
extern int yyleng;
extern int yylineno;

#endif 