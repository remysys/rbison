INCLUDE = ../include
CC = gcc
CFLAGS = -I$(INCLUDE) -DMAIN -g

lex.yy.c: parser.l
	rlex -vl parser.l

test: lex.yy.c
	gcc -o parser lex.yy.c -llex

clean:
	rm -fr *.o
	rm -fr lex.yy.c


