#include "parser.h"

void make_token_file()
{
  /* this subroutine generates the yytokens.h file. a special token
   * named _EOI_ (with a value of 0) is also generated
   */
  
  FILE *tokfile;
  int i;

  if (!(tokfile = fopen(TOKEN_FILE, "w"))) {
    error(FATAL, "can't open %s\n", TOKEN_FILE);
  } else if (Verbose) {
    printf("generating %s\n", TOKEN_FILE);
  }

  fprintf(tokfile, "#define _EOI_   0\n");
  for (i = MINTERM; i <= Cur_term; i++) {
    fprintf(tokfile, "#define %-10s %d\n", Terms[i]->name, (i - MINTERM) + 1);
  }

  fclose(tokfile);
}