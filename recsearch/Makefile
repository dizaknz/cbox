INCDIR=./
LIBDIR=./
CC=gcc
CFLAGS=-c -Wall -I$(INCDIR)
LFLAGS=-L$(LIBDIR)
SRCDIR=./src
OUTDIR=./bin

all: test

test: log.o table.o record.o test.o
	$(CC) $(LFLAGS) $(SRCDIR)/*.o -o $(OUTDIR)/test

log.o:
	$(CC) $(CFLAGS) $(SRCDIR)/log.c -o $(SRCDIR)/log.o

table.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/table.c -o $(SRCDIR)/table.o

record.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/record.c -o $(SRCDIR)/record.o

test.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/test.c -o $(SRCDIR)/test.o

clean:
	rm -rf $(SRCDIR)/*.o $(OUTDIR)/test

