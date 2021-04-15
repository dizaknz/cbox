INCDIR=./
LIBDIR=./
CC=gcc
CFLAGS=-c -Wall -I$(INCDIR) -Werror 
LFLAGS=-L$(LIBDIR)
SRCDIR=./src
OUTDIR=./bin

all: crcsearch

crcsearch: crcsearch.o
	$(CC) $(LFLAGS) $(SRCDIR)/crcsearch.o -o $(OUTDIR)/crcsearch

crcsearch.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/crcsearch.c -o $(SRCDIR)/crcsearch.o

clean:
	rm -rf $(SRCDIR)/*.o $(OUTDIR)/crcsearch 
