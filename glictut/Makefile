INCDIR=./
LIBDIR=./
CC=gcc
CFLAGS=-c -Wall -I$(INCDIR)
LFLAGS=-L$(LIBDIR)
SRCDIR=./src
OUTDIR=./bin

all: hstab btree

hstab: hstab.o
	$(CC) $(LFLAGS) $(SRCDIR)/hstab.o -o $(OUTDIR)/hstab

hstab.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/hstab.c -o $(SRCDIR)/hstab.o

btree: btree.o
	$(CC) $(LFLAGS) $(SRCDIR)/btree.o -o $(OUTDIR)/btree

btree.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/btree.c -o $(SRCDIR)/btree.o

clean:
	rm -rf $(SRCDIR)/*.o $(OUTDIR)/hstab $(OUTDIR)/btree

