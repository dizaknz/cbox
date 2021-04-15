#ifndef __HEAP_H
#define __HEAP_H

#include "record.h"

typedef struct heap heap;

struct heap {
    record *data;
    int elements;
};

heap *heap_new(int);
void heap_add(heap *, record *);
void heap_remove(heap *, record *);
void heap_grow (heap *, int);
heap *heap_heapify (list *);

#endif
