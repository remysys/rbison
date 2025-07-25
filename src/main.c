
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <compiler.h>
#include <l.h>

#define  ALLOCATE
#include "parser.h"
#undef   ALLOCATE

static int Warn_exit        = 0;      /* set to 1 if -W on command line */
static int Num_warnings     = 0;      /* total warnings printed */
static char *Output_fname   = "????"; /* name of the output file */
static FILE *Doc_file       = NULL;   /* error log & machine description */

#define RBISON_VERSION "0.02"

#define VERBOSE(str)  if (Verbose) { printf( "%s:\n", (str)); } else

/*
 * subroutine: 
 * file_header()  yydriver.c
 * code_header()  yydriver.c
 * driver()       yydriver.c
 * tables()       yycode.c
 * patch()        yypatch.c
 * do_dollar()    yydollar.c
 */


void onintr(int signum) /* SIGINT (ctrl-break, ^C) hanlder */
{
  if (Output != stdout) {
    fclose(Output);
    remove(Output_fname);
  }
  exit(EXIT_USR_ABRT);
}

void signon()
{
  /* print the sign-on message. since the console is opened explicitly, the
   * message is printed even if both stdout and stderr are redirected.
   */
  FILE *screen;
  if (!(screen = fopen("/dev/tty", "w"))) {
    screen = stderr;
  }

  fprintf(screen, "rbison %s [gcc %s] [%s]. (c) %s, ****.", RBISON_VERSION, __VERSION__, __DATE__,  __DATE__ + 7);
  fprintf(screen," all rights reserved.\n");

  if (screen != stderr) {
    fclose(screen);
  }
}

void lerror(int fatal, char *fmt, ...)
{
  /* this error-processing routine automatically generates a line number for
   * the error. If "fatal" is true, exit() is called.
   */
  
  va_list args;
  extern int yylineno;
  if (fatal == WARNING) {
    ++Num_warnings;
    if (No_warnings) {
      return;
    }
    fprintf(stdout, "%s WARNING (%s, line %d): ", PROG_NAME, Input_file_name, yylineno);
  } else if (fatal != NOHDR) {
    ++yynerrs;
    fprintf(stdout, "%s ERROR (%s, line %d): ", PROG_NAME, Input_file_name, yylineno);
  }

  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  fflush(stdout);
  
  if (Verbose && Doc_file) {
    if (fatal != NOHDR) {
      fprintf(Doc_file, "%s (line %d) ", fatal == WARNING ? "WARNING" : "ERROR", yylineno);
      vfprintf(Doc_file, fmt, args);
    }
  }

  if (fatal == FATAL) {
    exit(EXIT_OTHER);
  }
}

void error(int fatal, char *fmt, ...)
{
  /* this error routine works like lerror() except that no line number is
   * generated. the global error count is still modified, however.
   */

  va_list args;
  char *type;
  if (fatal == WARNING) {
    ++Num_warnings;
    if (No_warnings) {
      return;
    }
    type = "WARNING: ";
    fprintf(stdout, type);
  } else if (fatal != NOHDR) { /* if NOHDR is true. just act like printf*/
    ++yynerrs;
    type = "ERROR: ";
    fprintf(stdout, type);
  }

  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  fflush(stdout);

  if (Verbose && Doc_file) {
    va_start(args, fmt);
    fprintf(Doc_file, type);
    vfprintf(Doc_file, fmt, args);
  }

  if (fatal == FATAL) {
    exit(EXIT_OTHER);
  }
}

void parse_args(int argc, char *argv[])
{
  /* parse the command line, setting gloabl variables as appropriate */
  char *p;
  
  static char *usage_msg[] = {
    "usage is: rbison [-options] file",
    "",
    "  create an LALR(1) parser from the specification in the",
    "  input file. legal command-line options are:",
    "",
    "-a   output actions only (see -p)",
    "-l   suppress #(L)ine directives",
    "-p   output parser only (can be used with -T also)",
    "-s   make (s)ymbol table",
    "-S   make more-complete (S)ymbol table",
    "-t   print all (T)ables (and the parser) to standard output",
    "-T   move large tables from y.tab.c to y.outab.c",
    "-v   print (V)erbose diagnostics (including symbol table)",
    "-V   more verbose than -v. implies -t, & y.output goes to stderr",
    "-w   suppress all warning messages",
    "-W   warnings (as well as errors) generate nonzero exit status",
    NULL
  };

  /* note that all global variables set by command-line switches are declared
   * in parser.h. space is allocated because a #define ALLOC is present at
   * the top of the current file.
   */
  
  for (++argv, --argc; argc && *(p = *argv) == '-'; ++argv, --argc) {
    while (*++p) {
      switch (*p) {
        case 'a': Make_parser = 0;
                  Template = ACT_TEMPL;
                  break;
        case 'g': Public = 1;       break;
        case 'l': No_lines = 1;     break;
        case 'p': Make_actions = 0; break;
        case 's': Symbols = 1;      break;
        case 'S': Symbols = 2;      break;
        case 't': Use_stdout  = 1;  break;
        case 'T': Make_yyoutab = 1; break;
        case 'v': Verbose = 1;      break;
        case 'V': Verbose = 2;      break;
        case 'w': No_warnings = 1;  break;
        case 'W': Warn_exit = 1;    break;
        default:
          fprintf(stderr, "<-%c>: illegal argument\n", *p);
          printv(stderr, usage_msg);
          exit(EXIT_ILLEGAL_ARG);
      }
    }
  }

  if (Verbose > 1) {
    Use_stdout = 1;
  }

  if (argc <= 0) { 
    printv(stderr, usage_msg);
    exit(EXIT_ILLEGAL_ARG);
  } else if (argc > 1) {
    fprintf(stderr, "too many arguments\n");
    printv(stderr, usage_msg);
    exit(EXIT_TOO_MANY);
  } else { /* argc == 1, input from file */
    if (ii_newfile(Input_file_name = *argv) < 0) {
      error(FATAL, "can't open input file %s\n", *argv);
    }
  }
}

void symbols()  /* print the symbol table */
{
  FILE *fd;
  if (!(fd = fopen(SYM_FILE, "w"))) {
    perror(SYM_FILE);
  } else {
    print_symbols(fd);
    fclose(fd);
  }
}

void statistics(FILE *fp)
{
  /* print various statistics */

  int conflicts;  /* number of parse-table conflicts */
  
  if (Verbose) {
    fprintf(fp, "\n");
    fprintf(fp, "%4d/%-4d terminals\n",    USED_TERMS, NUMTERMS);
    fprintf(fp, "%4d/%-4d nonterminals\n", USED_NONTERMS, NUMNONTERMS);
    fprintf(fp, "%4d/%-4d productions\n",  Num_productions, MAXPROD);
    lr_stats(fp);
  }

  conflicts = lr_conflicts(fp);

  if (fp == stdout) {
    fp = stderr;
  }

  if ((Num_warnings - conflicts) > 0) {
    fprintf(fp, "%4d    warnings\n", Num_warnings - conflicts);
  }

  if (yynerrs) {
    fprintf(fp, "%4d    hard errors\n", yynerrs);
  }
  
}


void output(char *fmt, ...)
{
  /* works like printf(), but writes to the output file. see also: the
   * outc() macro in parser.h
   */
  
  va_list args;
  va_start(args, fmt);
  vfprintf(Output, fmt, args);
}

void document(char *fmt, ...)
{
  /* 
   * works like printf() but writes to y.output (provided that the file is being created)
   */
  
  va_list args;
  if (Doc_file) {
    va_start(args, fmt);
    vfprintf(Doc_file, fmt, args);
  }
}

void document_to(FILE *fp)
{
  /* change document-file output to the indicated stream, or to previous
   * stream if fp == NULL.
   */
  static FILE *oldfp;

  if (fp) {
    oldfp = Doc_file;
    Doc_file = fp;
  } else {
    Doc_file = oldfp;
  }
}

static void tail()
{
  /* copy the remainder of input file to standard output. yyparse will have
   * terminated with the input pointer just past the %%. attribute mapping
   * ($$ to Yyval, $N to a stack reference, etc.) is done by the do_dollar()
   * call
   *
   * on entry, the parser will have read one token too far, so the first
   * thing to do is print the current line number and lexeme.
   */

  extern int yylineno;  /* lex generated */
  extern char *yytext;  /* lex generated */
  int c;

   /* output newline following %% . note that yytext and yyleng are undefined at this point because
    * they were modified in yyparse() when input was accepted, below. you must use ii_text(), etc.,
    * to put them into a reasonable condition if you expect to access them in tail()
    */
  output("%s", ii_text());
  
  if (!No_lines) {
    output("\n#line %d \"%s\"\n", yylineno, Input_file_name);
  }

  ii_unterm();  /* lex will have terminated yytext */
  
  while ((c = ii_advance()) != 0) {
    if (c == -1) {
      ii_flush(1);
      continue;
    } else {
      outc(c);
    }
  }
}

int do_file()
{
  /* process the input file. return the number of errors */

  struct timeval start_time, end_time;
  long time;
  void nows(void); /* declared in parser.l */
   
  gettimeofday(&start_time, NULL); /* initialize times now so that the difference */
  end_time = start_time;           /* between times will be 0 if we don't build the tables */

  init_acts();
  file_header();

  VERBOSE("parsing");

  nows();     /* make lex ignore white space until ws() is called */
  yyparse();  /* parse the entire input file */
  
  if (!yynerrs || problems()) { /* if no problems in the input file */
    VERBOSE("analyzing grammar");
    first();        /* find FIRST sets */
    code_header();  /* print various #define to output file */
    patch();        /* patch up the grammar and output the actions */  

    if (Make_parser) {
      VERBOSE("make tables");
      tables();     /* generate the tables */
    }

    gettimeofday(&end_time, NULL);
    VERBOSE("copying driver");
    
    driver();     /* the parser */
    
    if (Make_actions) {
      tail();     /* and the tail end of the source file */
    }
  }

  if (Verbose) {
    time = (end_time.tv_sec * 1000000) + end_time.tv_usec;
    time -= (start_time.tv_sec * 1000000) + start_time.tv_usec;
    printf("time required to make tables: %ld.%-03ld seconds\n", (time/1000000), (time%1000000)/1000);
  }

  return yynerrs;
}


int main(int argc, char *argv[])
{
  signon();                /* print sign on message */
  signal(SIGINT, onintr);  /* close output files on ctrl-break */
  parse_args(argc, argv);

  if (Make_parser) {
    if (Verbose == 1) {
      if (!(Doc_file = fopen(DOC_FILE, "w"))) {
        ferr("can't open log file %s\n", DOC_FILE);
      }
    } else if (Verbose > 1) {
      Doc_file = stderr;
    }
  }

  if (Use_stdout) {
    Output_fname = "/dev/tty";
    Output = stdout;
  } else {
    Output_fname = !Make_parser ? ACT_FILE : PARSE_FILE;
    if (!(Output = fopen(Output_fname, "w"))) {
      error(FATAL, "can't open output file %s : %s\n", Output_fname, strerror(errno));
    }
  }

  if ((yynerrs = do_file()) == 0) { /* do all the work */
    if (Symbols) {
      symbols(); /* print the symbol table */
    }

    statistics(stdout); /* and any closing-up statistics */
    
    if (Verbose && Doc_file) {
      statistics(Doc_file);
    }
  } else {
    if (Output != stdout) {
      fclose(Output);
      if (remove(Output_fname) == -1) {
        perror(Output_fname);
      }
    }
  }

  /* exit with  the number of hard errors (or, if -W was specified, the sum
   * of the hard errors and warnings) as an exit status. Doc_file and Output
   * are closed implicitly by exit()
   */
  
  exit(yynerrs + (Warn_exit ? Num_warnings : 0));
  
  return 0;
}

#ifdef NEVER 
int main(int argc, char *argv[]) {
  Output = stdout;
  init_acts();
  if (ii_newfile(argv[1]) < 0) {
      perror(argv[1]);
      return -1;
  }
  //nows();
  yyparse();
  print_symbols(stdout);
}
#endif