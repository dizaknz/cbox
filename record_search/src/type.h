#ifndef __TYPE_H
#define __TYPE_H

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef uint8 bool;
typedef uint64 key;
typedef uint32 bucket;

#define TRUE 1
#define FALSE 0

#ifndef NULL
#define NULL 0x0
#endif

typedef enum {
    STATUS_OK,
    STATUS_NOT_FOUND,
    STATUS_MEMORY_ERROR,
    STATUS_UNKNOWN
} status;

#endif
