#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "record.h"

int getScaledMeasurement (double measurement) {
    return (int)(measurement * PRECISION_SCALE);
}

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
