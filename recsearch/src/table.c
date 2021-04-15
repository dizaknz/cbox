#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "table.h"

bucket table_getBucket (key k) {
    return k % NUM_BUCKETS;
}

status table_addRecord (table *self, record *r) {
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

record *table_findRecord (table *self, key k) {
    int i = 0;
    node *n = NULL;
    record *r = NULL;

    i = self->getBucket (k);
    n = self->data[i];
    if (!n) {
        return NULL;
    }
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
    int i = 0;
    node *n = NULL, *p = NULL;

    i = self->getBucket (k);
    n = self->data[i];
    if (!n) {
        return STATUS_NOT_FOUND;
    }
    while (n) {
        if (compEQ (n->key, k)) {
            break;
        }
        n = n->next;
        p = n;
    }

    if (n->next && p) {
        p->next = n->next;
    } else if (n->next && !p) {
        self->data[i] = n->next;
        n->next = NULL;
    } else if (!n->next && p) {
        p->next = NULL;
    } else {
        self->data[i] = NULL;
    }

    record_destroy (&n->rec);
    free (n);

    return STATUS_OK;
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
    table *t = calloc (1, sizeof (table));
    if (!t) {
        LOG_ERROR(("Out of memory (table)"));
        return NULL;
    }

    t->data = calloc (NUM_BUCKETS, sizeof(node *));
    if (!t->data) {
        LOG_ERROR(("Out of memory (data)"));
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
