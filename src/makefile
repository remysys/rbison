INCLUDE = ../include
CC = gcc
CFLAGS = -I$(INCLUDE) -g

# - /usr/local/lib: default install location for rlex's libl.a
LIB_DIRS = -L../lib -L/usr/local/lib
LIBS = -ll ../lib/libcomp.a

OBJS = main.o acts.o lex.yy.o first.o stok.o yypatch.o yycode.o yydriver.o yystate.o yydollar.o

.PHONY: rbison lib test clean

all: rbison

lib:
	make -C ../lib

# bison with simplified grammar support
sbison: $(OBJS) lib llparser.o
	$(CC) -o $@ $(OBJS) llparser.o $(LIB_DIRS) $(LIBS)

# bison with full grammar support
fbison: $(OBJS) lib sbison yyout.o
	$(CC) -o $@ $(OBJS) yyout.o $(LIB_DIRS) $(LIBS)

# bison with full grammar support
rbison: $(OBJS) lib parser.o
	$(CC) -o $@ $(OBJS) parser.o $(LIB_DIRS) $(LIBS)

test: clean rbison test_lex expr_1 expr_2 expr_3 expr_4 expr_5

test_lex: lex.yy.c
	$(CC) -o $@ -DMAIN lex.yy.c $(LIB_DIRS) -ll $(CFLAGS)

expr_1:
	rlex -vl ../test/expr_1.l
	./rbison -vsl ../test/expr_1.y
	$(CC) -o $@ lex.yy.c y.tab.c $(LIB_DIRS) -ll $(CFLAGS)

expr_2:
	rlex -vl ../test/expr_2.l
	./rbison -vsl ../test/expr_2.y
	$(CC) -o $@ lex.yy.c y.tab.c $(LIB_DIRS) -ll $(CFLAGS)

expr_3:
	rlex -vl ../test/expr_3.l
	./rbison -vsl ../test/expr_3.y
	$(CC) -o $@ lex.yy.c y.tab.c $(LIB_DIRS) -ll $(CFLAGS)

expr_4:
	rlex -vl ../test/expr_4.l
	./rbison -vsl ../test/expr_4.y
	$(CC) -o $@ lex.yy.c y.tab.c $(LIB_DIRS) -ll $(CFLAGS)

expr_5:
	rlex -vl ../test/expr_5.l
	./rbison -vsl ../test/expr_5.y
	$(CC) -o $@ lex.yy.c y.tab.c $(LIB_DIRS) -ll $(CFLAGS)

clean:
	rm -fr *.o lex.yy.c rbison sbison fbison test_lex expr_1 expr_2 expr_3 expr_4 expr_5 y.tab.h y.tab.c y.output y.sym
	make -C ../lib clean

yypatch.o: yypatch.c parser.h
yycode.o:  yycode.c parser.h
yydriver.o: yydriver.c parser.h
yystate.o: yystate.c parser.h
yydollar.o: yydollar.c parser.h
llparser.o: llparser.c parser.h
parser.o: parser.c parser.h
lex.yy.o: lex.yy.c parser.h

lex.yy.c: parser.l
	rlex -vl parser.l

yyout.o: y.tab.c parser.h

y.tab.c: parser.y
	./sbison -vls parser.y
