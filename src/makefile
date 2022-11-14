INCLUDE = ../include
CC = gcc
CFLAGS = -I$(INCLUDE) -g

OBJS = main.o acts.o lex.yy.o first.o stok.o yypatch.o yycode.o yydriver.o yystate.o yydollar.o

lib:
	make -C ../lib

#simple bison
sbison: llparser.o $(OBJS) lib
	$(CC) -o $@ $(OBJS) llparser.o ../lib/libcomp.a -llex

#final bison
rbison:	$(OBJS) lib sbison yyout.o
	$(CC) -o $@ $(OBJS) yyout.o ../lib/libcomp.a -llex

test: lex.yy.c
	$(CC) -o parser lex.yy.c -llex $(CFLAGS)

calc:
	rlex -vl ../test/expr.l
	./rbison -vsl ../test/expr.y
	$(CC) -o $@ lex.yy.c yyout.c -llex $(CFLAGS)

clean:
	rm -fr *.o
	rm -fr lex.yy.c
	rm -fr rbison
	rm -fr sbison
	rm -fr parser
	rm -fr yyout.h
	rm -fr yyout.c
	rm -fr yyout.doc
	rm -fr yyout.sym
	make -C ../lib clean

yypatch.o:	yypatch.c	parser.h
yycode.o:	yycode.c	parser.h
yydriver.o:	yydriver.c	parser.h
yystate.o:	yystate.c	parser.h
yydollar.o:	yydollar.c	parser.h
llparser.o:	llparser.c	parser.h
lex.yy.o:	lex.yy.c	parser.h
lex.yy.c:	parser.l
	rlex -vl parser.l

yyout.o:	yyout.c parser.h
yyout.c: parser.y
	./sbison -vls parser.y
