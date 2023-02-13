#ifndef _COMPILER_H
#define _COMPILER_H

#define max(a,b) ( ((a) > (b)) ? (a) : (b))
#define min(a,b) ( ((a) < (b)) ? (a) : (b))

/* ---------------- rbison/lib/ferr.c ---------------- */
extern int ferr(char *fmt, ...);

/* ---------------- rbison/lib/escape.c ---------------- */
extern char *bin_to_ascii(int c, int use_hex);

/* ----------------rbison/lib/fputstr.c ---------------- */
extern void fputstr (char *str, int maxlen, FILE *fp);

/* ----------------rbison/lib/printv.c ---------------- */
extern void printv(FILE *fp, char *argv[]);
extern void comment(FILE *fp, char *argv[]);

/* ---------------------rbison/src/yydriver.c-----------*/
extern FILE *driver_1(FILE *output, int lines);
extern int driver_2(FILE *output, int lines);
#endif