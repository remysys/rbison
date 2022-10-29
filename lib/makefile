INCLUDE = ../include
LIBCOMP = libcomp.a

CC = gcc
CFLAGS = -I$(INCLUDE)

.PHONY: all clean

COMP_OBJ = escape.o ferr.o fputstr.o hash.o set.o

all: $(LIBCOMP)

$(LIBCOMP): $(COMP_OBJ)
	rm -fr $(LIBCOMP)
	ar rvs $(LIBCOMP) $(COMP_OBJ)


# the following routines are used by rbison, but not by the
# programs that rbison generate

escape.o:		escape.c
ferr.o: 		ferr.c
fputstr.o: 	fputstr.c
hash.o: 		hash.c
set.o: 			set.c

clean:
	rm -fr $(COMP_OBJ) $(LIBCOMP)