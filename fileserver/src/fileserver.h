#include <stdio.h>
#include <stdlib.h>

#define CLIENT_QUEUE 8
#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT 5001

/* FileServer definition */
typedef struct FileServer FileServer;

struct FileServer {
    char *ip;
    int port;
    char *storage;
    char *cache;
    char *filter;
    int socket;
    int (*run) (void);
    void (*fileHandler) (void *);
    void (*close) ();
};

/* init */
void FileServer_init (const char *ip, int port, const char *storage, const char *filter);

/* cleanup */
void FileServer_close ();

/* run loop */
int FileServer_run (void);

/* request handler */
void FileServer_fileHandler (void *);
