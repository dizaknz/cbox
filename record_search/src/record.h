#ifndef __RECORD_H
#define __RECORD_H

#include <time.h>
#include "type.h"

/* retain 2 decimal precision */
#define PRECISION_SCALE 100

/* length of record key allowed */
#define KEY_LENGTH 1024

typedef struct record record;

/* @todo maybe create generic record defined by meta data
   
   meta data:
   RECORD_TYPE enum
   RECORD_DATUM enum
   RECORD_UNPACK enum/func
   RECORD_CMP enum/func
 */
typedef enum recordType {
    PRIMITIVE,
    TUPLE
} recordType;

typedef enum recordDatum {
    /* PRIMITIVE */
    UINT64,
    INT64,
    /* TUPLE */
    STRUCT
} recordDatum;

struct record {
    void *data;
    int id;
    time_t t;
    double temp;
    double relhum;
    char * (*asString) (record *self);
    key (*getKey) (record *self);
};


int getScaledMeasurement (double measurement);

char * record_asString (record *self);
key record_getKey (record *self);
record * record_new ();
void record_destroy (record **self);

#endif
