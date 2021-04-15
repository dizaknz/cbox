INCDIR=/usr/include/openmpi-x86_64
LIBDIR=/usr/lib64/openmpi/lib
CC=mpicc
CFLAGS=-c -Wall -I$(INCDIR)
LFLAGS=-L$(LIBDIR) -lmpi
SRCDIR=./src
OUTDIR=./bin

all: multiproc

multiproc: multiproc.o
	$(CC) $(LFLAGS) $(SRCDIR)/multiproc.o -o $(OUTDIR)/multiproc

multiproc.o: 
	$(CC) $(CFLAGS) $(SRCDIR)/multiproc.c -o $(SRCDIR)/multiproc.o

clean:
	rm -rf $(SRCDIR)/*.o $(OUTDIR)/multiproc

