INCDIR=
LIBDIR=
CC=gcc
CFLAGS=-c -Wall -I$(INCDIR)
LFLAGS=-L$(LIBDIR)
SRCDIR=./src
OUTDIR=./bin

all: test

test: test.o
	$(CC) $(LFLAGS) $(SRCDIR)/test.o -o $(OUTDIR)/test

test.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/test.c -o $(SRCDIR)/test.o

clean:
	rm -rf $(OUTDIR)/*.o $(OUTDIR)/test

