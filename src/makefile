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

.PHONY: test

test:	clean rbison test_lex expr_1 expr_2 expr_3 expr_4 expr_5

test_lex: lex.yy.c
	$(CC) -o $@ -DMAIN lex.yy.c -llex $(CFLAGS)

expr_1:	
	rlex -vl ../test/expr_1.l
	./rbison -vsl ../test/expr_1.y
	$(CC) -o $@ lex.yy.c yyout.c ../lib/libcomp.a -llex $(CFLAGS)

expr_2:	
	rlex -vl ../test/expr_2.l
	./rbison -vsl ../test/expr_2.y
	$(CC) -o $@ lex.yy.c yyout.c ../lib/libcomp.a -llex $(CFLAGS)

expr_3:	
	rlex -vl ../test/expr_3.l
	./rbison -vsl ../test/expr_3.y
	$(CC) -o $@ lex.yy.c yyout.c ../lib/libcomp.a -llex $(CFLAGS)

expr_4:	
	rlex -vl ../test/expr_4.l
	./rbison -vsl ../test/expr_4.y
	$(CC) -o $@ lex.yy.c yyout.c ../lib/libcomp.a -llex $(CFLAGS)

expr_5:	
	rlex -vl ../test/expr_5.l
	./rbison -vsl ../test/expr_5.y
	$(CC) -o $@ lex.yy.c yyout.c ../lib/libcomp.a -llex $(CFLAGS)

clean:
	rm -fr *.o
	rm -fr lex.yy.c
	rm -fr rbison
	rm -fr sbison
	rm -fr test_lex
	rm -fr expr_1
	rm -fr expr_2
	rm -fr expr_3
	rm -fr expr_4
	rm -fr expr_5
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
