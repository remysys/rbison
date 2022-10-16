#ifndef _COMPILER_H
#define _COMPILER_H


/* ---------------- rbison/lib/ferr.c ---------------- */
int ferr(char *fmt, ...);

/* ---------------- rbison/lib/escape.c ---------------- */
char *bin_to_ascii(int c, int use_hex);

/* ----------------rbison/lib/printutils.c ---------------- */
void fputstr (char *str, int maxlen, FILE *fp);

#endif