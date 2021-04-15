#include <time.h>

typedef unsigned long key;

/* retain 2 decimal precision */
#define PRECISION_SCALE 100

/* length of record key allowed */
#define KEY_LENGTH 1024

typedef struct record record;

typedef struct record {
    int id;
    time_t t;
    double temp;
    double relhum;
    char * (*asString) (record *self);
    key (*getKey) (record *self);
} record;


int getScaledMeasurement (double measurement);

char * record_asString (record *self);
key record_getKey (record *self);
record * record_new ();
void record_destroy (record **self);
