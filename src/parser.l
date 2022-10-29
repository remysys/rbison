%{
#include <stdio.h>
#include "llout.h"
  
#define CREATING_LALR_PARSER /* suppress various definitions in parser.h that conflict with rlex-generated defs */
#include "parser.h"


/* lexical analyzer for rbison. 
 * whitespace, comments, and otherwise illegal characters must be handled
 * specially. when we're processing code blocks, we need to get at
 * the characters so that they can be passed to the output, otherwise, the
 * characters should be ignored. the ws() and nows() subroutines (at the
 * bottom of the file) switch between these behaviors by changing the value
 * if Ignore. If Ignore is true, white space is ignored.
 */

static int Ignore = 0; 
static int Start_line; /* starting line number */

/* 
 * prototypes for functions at the bottom of this file: 
 */

void nows(void);                            /* ignore white space, etc */
void ws(void);                              /* don't ignore white space, etc */
void lerror(int status, char *fmt, ...);
void output(char *fmt, ...);
%}

c_name  [A-Za-z_][A-Za-z_0-9]*
%%

"/*"  { /* absorb a comment (treat it as WHITESPACE) */
        int i;
        int start = yylineno;

        while (i = input()) {
          if (i < 0) {
            ii_unterm();
            ii_flush(1);
            ii_term();
            lerror(NONFATAL, "commnet starting on line ");
            lerror(NOHDR, "%d too long, truncating\n", start);
          } else if (i == '*' && ii_lookahead(1) == '/') {
            input();
            if (Ignore) {
              goto end;
            } else {
              return WHITESPACE;
            }
          }
        }
        lerror(FATAL, "end of file encountered in comment\n");
        
        end:; 
      }

\{  {
      int i;
      int nestlev;    /* brace-nesting level */
      int lb1;        /* previous character */
      int lb2;        /* character before that */
      int in_string;  /* processing string constant */
      int in_char_const; /* processing char constant */
      int in_comment;    /* processing a comment */

      lb1 = lb2 = 0;
      in_string = 0;
      in_char_const = 0;
      in_comment = 0;
      Start_line = yylineno;

      for (nestlev = 1; i = input(); lb2 = lb1, lb1 = i) {
        if (lb2 == '\n' && lb1 == '%' && i == '%') {
          lerror(FATAL, "%%%% in code block starting on line %d\n", Start_line);
        }

        if (i < 0) {
          ii_unterm();
          ii_flush(1);
          ii_term();
          lerror(FATAL, "code block starting on line %d too long\n", Start_line);
        }

        if (i == '\n' && in_string) {
          lerror(WARNING, "newline in string, inserting \"\n");
          in_string = 0;
        }

        /* take case of \{, "{", '{', \}, "}", '}' */
        if (i == '\\') {
          i = input();
          if (i <= 0) { /* discard backslash */
            break;
          } else {
            continue;
          }
        }

        if (i == '"' && !(in_char_const || in_comment)) {
          in_string = !in_string;
        } else if (i == '\'' && !(in_string || in_comment)) {
          in_char_const = !in_char_const;
        } else if (lb1 = '/' && i == '*' && !in_string) {
          in_comment = 1;
        } else if (lb1 = '*' && i == '/' && in_comment) {
          in_comment = 0;
        }

        if (!(in_string || in_char_const || in_comment)) {
          if (i == '{') {
            ++nestlev;
          }

          if (i == '}' && --nestlev <= 0) {
            return ACTION;
          }
        }
      }

      lerror(FATAL, "eof in code block starting on line %d\n", Start_line);
    }

^"%%"	return SEPARATOR;

"%{"[\s\t]* {
              /* copy a code block to the output file */
              int c, looking_for_brace = 0;
              #undef output /* replace macro with function in main.c */
              if (!No_lines) {
                output("\n#line %d \"%s\"\n", yylineno, Input_file_name);
              }

              while (c = input()) { /* while not at end of file */
                if (c == -1) { /* buffer is full, flush it */
                  ii_flushbuf();
                } else {
                  if (looking_for_brace) { /* last char was a % */
                    if (c == '}') {
                      break;
                    } else {
                      output("%%%c", c);
                      looking_for_brace = 0;
                    }
                  } else {
                    if (c == '%') {
                      looking_for_brace = 1;
                    } else {
                      output("%c", c);
                    }
                  }
                }
              }
	            return CODE_BLOCK;
            }
 
<{c_name}>  return FIELD;
"%union"    return PERCENT_UNION;
"%token"    |
"%term"     return TERM_SPEC;
"%type"     return TYPE;
"%left"     return LEFT;
"%right"    return RIGHT;
"%nonassoc" return NONASSOC;
"%prec"	    return PREC;
"%start"    return START;
":"         return COLON;
"|"         return OR;
";"         return SEMI;

[^\x00-\s%\{}[\]()*:|;,<>]+     return NAME;
[\x00-\s] { if (!Ignore) { 
              return WHITESPACE;
            } 
          }
%%

void nows() { Ignore = 1; }   /* ignore white space */
void ws() { Ignore = 0; }     /* don't ignore white space */ 

int start_action() /* return starting line number of most recently read ACTION block */
{
  return Start_line;
}

#ifdef MAIN	/* test routine,  reads lexemes and prints them */

int No_lines = 0;
char *Input_file_name;
FILE *Output;

#include <stdarg.h>
#include <stdlib.h>

void plex(int lex) {
  switch(lex) {
    case ACTION:        printf("ACTION (%s)\n", yytext);          break;
    case CODE_BLOCK:    printf("CODE_BLOCK (%s)\n", yytext);      break;
    case COLON:         printf("COLON (%s)\n", yytext);           break;
    case FIELD:         printf("FIELD (%s)\n", yytext);           break;
    case LEFT:          printf("LEFT (%s)\n", yytext);            break;
    case NAME:          printf("NAME (%s)\n", yytext);            break;
    case NONASSOC:      printf("NONASSOC (%s)\n", yytext);        break;
    case OR:            printf("OR (%s)\n", yytext);              break;
    case PERCENT_UNION: printf("PERCENT_UNION (%s)\n", yytext);   break;
    case PREC:          printf("PREC (%s)\n", yytext);            break;
    case RIGHT:         printf("RIGHT (%s)\n", yytext);           break;
    case SEMI:          printf("SEMI (%s)\n", yytext);            break;
    case SEPARATOR:     printf("SEPARATOR (%s)\n", yytext);       break;
    case START:         printf("START (%s)\n", yytext);           break;
    case TERM_SPEC:     printf("TERM_SPEC (%s)\n", yytext);       break;
    case TYPE:          printf("TYPE (%s)\n", yytext);            break;
    case WHITESPACE:    printf("WHITESPACE (%s)\n", yytext);      break;
    default:            printf("*** unknown *** (%s)\n",yytext);  break;
	}
}

int main(int argc, char *argv[])
{
  int lex;
  Output = stdout;
  nows();

  if (1 == argc) {
    while(lex = yylex()) {
      plex(lex);
    }
  } else {
    if (ii_newfile(Input_file_name = argv[1]) < 0) {
      perror(argv[1]);
    } else {
      while(lex = yylex()) {
        plex(lex);
      }
    }
  }
}

void output(char *fmt, ...) 	/* the real versions of these subroutines are in main.c.  */
{	
  /* these stubs are here for debugging a standalone */
  va_list args;
  va_start(args, fmt);
  vfprintf(Output, fmt, args);
  fflush(Output);
}

void lerror(int status, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  if (status == FATAL) {
    exit(1);
  }
}
#endif
