#ifndef __CRCSEARCH_H_
#define __CRCSEARCH_H_

#include <stdio.h>
#include <stdlib.h>

#define SUCCESS 0
#define ERROR 1

#define TABSIZE 256
#define NUMBITS 8
#define BUFSIZE 256

typedef unsigned char byte;
typedef unsigned long long uint64;
typedef struct CRCSearch CRCSearch;

struct CRCSearch {
    /* lookup table */
    uint64 table[TABSIZE];
    byte found;
    uint64 length;
    void (*init) (CRCSearch *, uint64);
    uint64 (*search) (CRCSearch *, const char*, uint64);
    void (*close) (CRCSearch **);
};

CRCSearch * CRCSearch_new (uint64);
void CRCSearch_init(CRCSearch *, uint64);
uint64 CRCSearch_search(CRCSearch *, const char *, uint64);
void CRCSearch_close (CRCSearch **);

#endif
