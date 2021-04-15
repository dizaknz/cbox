#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "table.h"
#include "log.h"

int testAddAndRemove(void) {
    int failed = 0;
    table *t = table_new();

    record *r1 = record_new (1, 1111220202, 11.2, 89.90);
    char *s1 = r1->asString(r1);
    t->addRecord (t, r1);

    record *r2 = record_new (2, 1111220203, 11.2, 89.90);
    char *s2 = r2->asString(r2);
    t->addRecord (t, r2);

    record *r3 = record_new (3, 1111220204, 11.2, 89.90);
    char *s3 = r3->asString(r3);
    key k3 = r3->getKey(r3);

    t->addRecord (t, r3);

    fprintf (stdout, "DEBUG: printing out table\n");
    t->print(t);

    fprintf (stdout, "DEBUG: removing record with key: %lu\n", k3);
    status s = t->removeRecord (t, k3);
    if (s != STATUS_OK) {
        fprintf (stderr, "ERROR: could not remove record by key: %lu\n", k3);
        failed++;
    }

    fprintf (stdout, "DEBUG: printing out table\n");
    t->print(t);

    free(s1);
    free(s2);
    free(s3);

    table_destroy(t);

    return failed;
}

int main (int argc, char **argv) {
    return testAddAndRemove();
}
