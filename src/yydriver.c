#include <stdio.h>
#include "parser.h"

static FILE *Driver_file;

static FILE *Input_file = NULL;        /* rbison.par default */
static int Input_line;                 /* line number of most-recently read line */
static char *File_name = "rbison.par"; /* template file name */

FILE *driver_1(FILE *output, int lines) 
{
	
  if (!(Input_file = fopen(File_name, "r"))) {
    return NULL;
  }

  Input_line = 0;
  driver_2(output, lines);
  return Input_file;
}

int driver_2(FILE *output, int lines)
{
  static char buf[256];
  char *p;
  int processing_comment = 0;
  if (!Input_file) {
    ferr("internal error [driver_2], template file %s not open\n", File_name);
  }

  if (lines) {
    fprintf(output, "\n#line %d \"%s\"\n", Input_line + 1, File_name);
  }

  while(fgets(buf, sizeof(buf), Input_file)) {
    ++Input_line;
    if (*buf == '?') {
      break;
    }

    for (p = buf; isspace(*p); ++p) {
      ;
    }

    if (*p == '@') {
      processing_comment = 1;
      continue;
    } else if (processing_comment) {  /* previous line was a comment */
      /* but current line is not */
      processing_comment = 0;
      if (lines) {
        fprintf(output, "\n#line %d \"%s\"\n", Input_line, File_name);
      }
    }

    fputs(buf, output);
  }

  return feof(Input_file);
}

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

  if (!(Driver_file = driver_1(Output, !No_lines))) {
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