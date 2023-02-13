#include <stdio.h>
#include <compiler.h>
#include "parser.h"


void make_yy_stok()
{
  /* this subroutine generates the Yy_stok[] array that's
   * indexed by token value and evaluates to a string
   * representing the token name. token values are adjusted
   * so that the smallest token value is 1 (0 is reserved
   * for end of input)
   */
  int i;
  static char *the_comment[] = {
    "Yy_stok[] is used for error messages. it is indexed",
    "by the internal value used for a token (as used for a column index in",
    "the transition matrix) and evaluates to a string naming that token",
  };

  comment(Output, the_comment);

  output("char *Yy_stok[] = \n{\n");
  output("  /*  %3d */  \"%s\",\n", 0, "_EOI_");

  for (i = MINTERM; i <= Cur_term; i++) {
    output("  /*  %3d */  \"%s\"", (i - MINTERM) + 1, Terms[i]->name);
    if (i != Cur_term) {
      output(",\n");
    }
  }

  output("\n};\n\n");
}


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

  fprintf(tokfile, "#define %-20s %d\n", "_EOI_", 0);
  for (i = MINTERM; i <= Cur_term; i++) {
    fprintf(tokfile, "#define %-20s %d\n", Terms[i]->name, (i - MINTERM) + 1);
  }

  fclose(tokfile);
}