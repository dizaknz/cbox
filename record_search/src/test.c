#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "table.h"

int main (int argc, char **argv) {
    table *t = table_new();

    record *r1 = record_new (1, 1111220202, 11.2, 89.20);
    char *s1 = r1->asString(r1);
    key k1 = r1->getKey(r1);

    fprintf (stdout, "DEBUG: bucket=%d key=%llu rec=%s\n", t->getBucket(k1), k1, s1);

    t->addRecord (t, r1);

    record *r2 = record_new (2, 1111220202, 11.2, 89.20);
    char *s2 = r2->asString(r2);
    key k2 = r2->getKey(r2);

    fprintf (stdout, "DEBUG: bucket=%d key=%llu rec=%s\n", t->getBucket(k2), k2, s2);

    t->addRecord (t, r2);

    record *r3 = record_new (3, 1111220203, 11.2, 89.20);
    char *s3 = r3->asString(r3);
    key k3 = r3->getKey(r3);

    fprintf (stdout, "DEBUG: bucket=%d key=%llu rec=%s\n", t->getBucket(k3), k3, s3);

    t->addRecord (t, r3);

    fprintf (stdout, "DEBUG: printing out table\n");
    t->print(t);

    fprintf (stdout, "DEBUG: removing record with key: %llu\n", k3);
    t->removeRecord (t, k3);

    fprintf (stdout, "DEBUG: printing out table\n");
    t->print(t);

    free(s1);
    free(s2);
    free(s3);

    table_destroy(t);

    return 0;
}
