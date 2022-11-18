rbison: a clear version of the GNU utility bison
===

rbison takes as input a context-free grammar specification and produces a LALR(1) parser that recognizes correct instances of the grammar. you can use it to develop a wide range of language parsers, from those used in simple calculators to complex programming languages.

![rbison internals](doc/rbison.png)

Usage
-----------
```
$ cd src && make rbison
$ ./rbison 
rbison 0.01 [gcc 4.8.5] [Nov 19 2022]. (c) 2022, ****. all rights reserved.
usage is: rbison [-options] file

  create an LALR(1) parser from the specification in the
  input file. legal command-line options are:

-a   output actions only (see -p)
-l   suppress #(L)ine directives
-p   output parser only (can be used with -T also)
-s   make (s)ymbol table
-S   make more-complete (S)ymbol table
-t   print all (T)ables (and the parser) to standard output
-T   move large tables from y.tab.c to y.outab.c
-v   print (V)erbose diagnostics (including symbol table)
-V   more verbose than -v. implies -t, & y.output goes to stderr
-w   suppress all warning messages
-W   warnings (as well as errors) generate nonzero exit status
```

run 'make test' to compile the usage samples in the test directory
```
$ make test
```
