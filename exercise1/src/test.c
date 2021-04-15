#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

typedef unsigned long key;
typedef unsigned int bucket;

/* retain 2 decimal precision */
#define PRECISION_SCALE 100

/* default number of buckets */
#define NUM_BUCKETS 1024

/* length of record key allowed */
#define KEY_LENGTH 1024

#define compEQ(a,b) (a == b)

int getScaledMeasurement (double measurement) {
    return (int)(measurement * PRECISION_SCALE);
}

typedef struct record record;

typedef struct record {
    int id;
    time_t t;
    double temp;
    double relhum;
    char * (*asString) (record *self);
    key (*getKey) (record *self);
} record;

char * record_asString (record *self) {
    char *str = (char *)calloc (1024, sizeof (char));

    sprintf (
        str,
        "{ id:%d, t:%zu, temp:%g, relhum:%g }",
        self->id,
        self->t,
        self->temp,
        self->relhum
    );

    return str;
}

key record_getKey (record *self) {
    key k = 5;

    k = 17 * k + (long)self->t;
    k = 17 * k + getScaledMeasurement (self->temp),
    k = 17 * k + getScaledMeasurement (self->relhum);

    if (0) {
        /* not used - dummy implementation */
        char tag[KEY_LENGTH] = { '\0' };
        char *e = NULL;
        long val = 0;

        /* convert tag into key */
        snprintf (
            tag, 
            KEY_LENGTH,
            "%ld%d%d",
            ((long)self->t),
            getScaledMeasurement (self->temp),
            getScaledMeasurement (self->relhum)
        );

        val = strtol (tag, &e, 10);
        k = abs ((int)(val ^ (val >> 32)));
    }

    return k;
}

record * record_new (
    int id,
    time_t t,
    double temp,
    double relhum
) {
    record *r = calloc (1, sizeof (record));

    if (!r) {
        fprintf (stderr, "Out of memory (record)\n");
        return NULL;
    }

    r->id = id;
    r->t = t;
    r->temp = temp;
    r->relhum = relhum;
    r->asString = &record_asString;
    r->getKey = &record_getKey;

    return r;
}

void record_destroy (record **self) {
    if (*self) {
        free (*self);
    }
    *self = NULL;

    return;
}

typedef enum {
    STATUS_OK,
    STATUS_NOT_FOUND,
    STATUS_MEMORY_ERROR,
    STATUS_UNKNOWN
} status;

typedef struct node node;

typedef struct node {
    key key;
    record *rec;
    node *next;
} node;

typedef struct table table;

typedef struct table {
    node **data;
    bucket (*getBucket) (key k);
    status (*addRecord) (table *self, record *r);
    status (*removeRecord) (table *self, key k);
    record *(*findRecord) (table *self, key k);
    void (*clean) (table *self);
    void (*print) (table *self);
} table;

bucket table_getBucket (key k) {
    return abs (k % NUM_BUCKETS);
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

    /* get bucket index */
    i = self->getBucket (k);

    /* iterate through node linked list and find record by key */
    n = self->data[i];

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

int main (int argc, char **argv) {
    table *t = table_new();

    record *r1 = record_new (1, 1111220202, 11.2, 89.20);
    char *s1 = r1->asString(r1);
    key k1 = r1->getKey(r1);

    fprintf (stdout, "DEBUG: bucket=%d key=%lu rec=%s\n", t->getBucket(k1), k1, s1);

    t->addRecord (t, r1);

    record *r2 = record_new (2, 1111220202, 11.2, 89.20);
    char *s2 = r2->asString(r2);
    key k2 = r2->getKey(r2);

    fprintf (stdout, "DEBUG: bucket=%d key=%lu rec=%s\n", t->getBucket(k2), k2, s2);

    t->addRecord (t, r2);

    record *r3 = record_new (3, 1111220203, 11.2, 89.20);
    char *s3 = r3->asString(r3);
    key k3 = r3->getKey(r3);

    fprintf (stdout, "DEBUG: bucket=%d key=%lu rec=%s\n", t->getBucket(k3), k3, s3);

    t->addRecord (t, r3);

    fprintf (stdout, "DEBUG: printing out table\n");
    t->print(t);

    fprintf (stdout, "DEBUG: removing record with key: %lu\n", k3);
    t->removeRecord (t, k3);

    fprintf (stdout, "DEBUG: printing out table\n");
    t->print(t);

    free(s1);
    free(s2);
    free(s3);

    table_destroy(t);

    return 0;
}
