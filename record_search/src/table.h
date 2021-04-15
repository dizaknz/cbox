#ifndef __TABLE_H
#define __TABLE_H

#include "list.h"

/* default number of buckets */
#define NUM_BUCKETS 1024

#define compEQ(a,b) (a == b)

typedef struct table table;

typedef struct table {
    /* @todo add list data */
    node **data;
    bucket (*getBucket) (key k);
    status (*addRecord) (table *self, record *r);
    status (*removeRecord) (table *self, key k);
    record *(*findRecord) (table *self, key k);
    void (*clean) (table *self);
    void (*print) (table *self);
} table;


bucket table_getBucket (key k);
status table_addRecord (table *self, record *r);
record *table_findRecord (table *self, key k);
status table_removeRecord (table *self, key k);
void table_clean (table *self);
void table_print (table *self);
table *table_new (void);
void table_destroy (table *self);

#endif
