#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "table.h"

/* TODO: how to make this thread safe */

bucket table_getBucket (key k) {
    return k % NUM_BUCKETS;
}

status table_addRecord (table *self, record *r) {
    /* @todo refactor into list.c or not and just add table_sortBucket, table_reverseBucket, etc */
    node *current, *new;
    int bucket;
    key k;

    k = r->getKey(r);
    bucket = self->getBucket(k);

    current = self->data[bucket];

    new = calloc (1, sizeof (node));

    if (!new) {
        return STATUS_MEMORY_ERROR;
    }

    self->data[bucket] = new;

    new->key = k;
    new->rec = r;
    new->next = current;
    
    return STATUS_OK;
}

/* each bucket could hold a generic data structure, depending on enum type of table
 * table with list, trees, etc
 */
record *table_findRecord (table *self, key k) {
    int i = 0;
    node *n = NULL;
    record *r = NULL;

    /* get bucket index */
    i = self->getBucket (k);

    /* iterate through node linked list and find record by key */
    n = self->data[i];

    /* @todo: optimize - binary search or tree search for table of lists of trees as search tree */
    while (n) {
        if (compEQ (n->key, k)) {
            r = n->rec;
            break;
        }
        n = n->next;
    }

    return r;
}

status table_removeRecord (table *self, key k) {
    status s = STATUS_OK;
    int i = 0;
    node *n = NULL, *p = NULL;

    /* get bucket index */
    i = self->getBucket (k);

    /* iterate through node linked list and find node by key */
    n = self->data[i];

    while (n) {
        if (compEQ (n->key, k)) {
            break;
        }
        n = n->next;
        p = n;
    }

    if (n) {
        if (n->next) {
            /* not the end, adjust the list */
            if (p) {
                /* has a previous node */
                p->next = n->next;
            }
        }

        if (!n->next && !p) {
            /* only one in list */
            self->data[i] = NULL;
        }

        record_destroy (&n->rec);
        free (n);
    }
    else {
        s = STATUS_NOT_FOUND;
    }

    return s;
}

void table_clean (table *self) {
    int i = 0;
    node *n = NULL, *tmp = NULL;

    for (i = 0; i < NUM_BUCKETS; i++) {
        n = self->data[i];
        while (n) {
            tmp = n;
            n = n->next;

            record_destroy (&tmp->rec);
            free (tmp);
        }
    }

    return;
}

void table_print (table *self) {
    int i = 0;
    node *n = NULL;
    char *s = NULL;

    for (i = 0; i < NUM_BUCKETS; i++) {
        n = self->data[i];
        if (n) {
            fprintf (stdout, "bucket=%d", i);

            /* populated bucket */
            while (n) {
                s = n->rec->asString(n->rec); 
                fprintf (stdout, " node=%p record=%s", n, s);
                free (s);

                n = n->next;
            }
            fprintf (stdout, "\n");
        }
    }
}

table *table_new (void) {
    /* allocate data table */
    table *t = calloc (1, sizeof (table));
    if (!t) {
        fprintf (stderr, "Out of memory (table)\n");
        return NULL;
    }

    /* allocate data table */
    t->data = calloc (NUM_BUCKETS, sizeof(node *));
    if (!t->data) {
        fprintf (stderr, "Out of memory (data)\n");
        return NULL;
    }

    t->getBucket = &table_getBucket;
    t->addRecord = &table_addRecord;
    t->removeRecord = &table_removeRecord;
    t->findRecord = &table_findRecord;
    t->clean = &table_clean;
    t->print = &table_print;

    return t;
}

void table_destroy (table *self) {
    if (self->data) {
        self->clean(self);
        free (self->data);
    }
    free (self);

    return;
}
