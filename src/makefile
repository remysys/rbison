INCLUDE = ../include
CC = gcc
CFLAGS = -I$(INCLUDE) -g

#CFLAGS = -I$(INCLUDE) -DMAIN

yylex.c: parser.lex
	rlex -vl parser.lex

test: yylex.c
	gcc -o parser -DMAIN yylex.c -llex
clean:
	rm -fr *.o
	rm -fr yylex.c


