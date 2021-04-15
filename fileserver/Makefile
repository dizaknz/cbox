INCDIR=./
LIBDIR=./
CC=gcc
CFLAGS=-c -Wall -I/opt/local/include -I$(INCDIR) -Werror -fstack-protector-all -Wstack-protector
LFLAGS=-L/opt/local/lib -L$(LIBDIR) -lpthread -lssl -lcrypto
SRCDIR=./src
OUTDIR=./bin

all: fileserver fileclient

fileserver: fileserver.o
	$(CC) $(LFLAGS) $(SRCDIR)/fileserver.o $(SRCDIR)/fileutils.o -o $(OUTDIR)/fileserver

fileserver.o: fileutils.o 
	$(CC) $(CFLAGS) $(SRCDIR)/fileserver.c -o $(SRCDIR)/fileserver.o

fileclient: fileclient.o
	$(CC) $(LFLAGS) $(SRCDIR)/fileclient.o $(SRCDIR)/fileutils.o -o $(OUTDIR)/fileclient

fileclient.o: fileutils.o
	$(CC) $(CFLAGS) $(SRCDIR)/fileclient.c -o $(SRCDIR)/fileclient.o

fileutils.o:
	$(CC) $(CFLAGS) $(SRCDIR)/fileutils.c -o $(SRCDIR)/fileutils.o

clean:
	rm -rf $(SRCDIR)/*.o $(OUTDIR)/fileserver $(OUTDIR)/fileclient
