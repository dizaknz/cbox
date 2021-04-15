INCDIR=./
LIBDIR=./
CC=gcc
CFLAGS=-c -Wall -I$(INCDIR)
LFLAGS=-L$(LIBDIR)
SRCDIR=./src
OUTDIR=./bin

all: test

test: test.o record.o table.o
	$(CC) $(LFLAGS) $(SRCDIR)/test.o $(SRCDIR)/record.o $(SRCDIR)/table.o -o $(OUTDIR)/test

table.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/table.c -o $(SRCDIR)/table.o

record.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/record.c -o $(SRCDIR)/record.o

test.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/test.c -o $(SRCDIR)/test.o

clean:
	rm -rf $(SRCDIR)/*.o $(OUTDIR)/test

