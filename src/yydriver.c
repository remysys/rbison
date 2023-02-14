#include <stdio.h>
#include <ctype.h>
#include <compiler.h>
#include "parser.h"

static FILE *Driver_file;

/* they MUST be called in the following order:
 * file_header()
 * code_header()
 * driver()
 */


void file_header()
{
  /* this header is printed at the top of the output file, before the
   * definitions section is processed. various #defines that you might want
   * to modify are put here
   */
  
  output("#include \"%s\"\n\n", TOKEN_FILE);
  
  if (Public) {
    output("#define PRIVATE\n");
  }

  if (Make_actions) {
    output("#define YYACTION\n");
  }

  if (Make_parser) {
    output("#define YYPARSER\n");
  }

  if (!(Driver_file = driver_1(Output, !No_lines, Template))) {
    error(NONFATAL, "rbison.par not found--output file won't compile\n");
  }

}


void code_header()
{
  /* this stuff is output after the definitions section is processed, but
   * before any tables or the driver is processed
   */

  driver_2(Output, !No_lines);
}

void driver()
{
  /* print out the actual parser by copying rbison.par to the output file */
  if (Make_parser) {
    driver_2(Output, !No_lines);
  }

  fclose(Driver_file);
}