#ifndef __LIST_H
#define __LIST_H

#include "type.h"
#include "record.h"

typedef struct node node;
typedef struct list list;

struct node {
    key key;
    record *rec;
    node *next;
};

struct list {
    node *start;
    node *end;
    int elements;
    bool isSorted;
    status (*add) (list *, record *);
    status (*remove) (list *, record *);
    status (*find) (list *, record *);
    void (*sort) (list *);
    void (*reverse) (list *);
    void (*isCircular) (list *);
    void (*isValid) (list *);
};

list * list_new ();
status list_add (list *, record *);
status list_remove (list *, record *);
status list_find (list *, record *);
void list_sort (list *);
void list_reverse (list *);
const record * list_asHeap(list *);
bool list_isCircular (list *); 

#endif
