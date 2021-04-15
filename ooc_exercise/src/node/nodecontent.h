/**
 * A node value is a type that has it's own unpacker
 */

typedef struct nodecontent nodecontent;

typedef enum datatype {
    STRING,
    INT8,
    INT16,
    INT32,
    TBINARY
};

struct nodecontent {
    void *data;
    int length;
    
    char (*unpack)(void);
    void (*destroy)(void);
}

/* constructor */
nodecontent *nodecontent_new (datatype type, void *data);

bool nodecontent_destroy (nodecontent *content);

/* sanity checks on content type and data provided */
bool nodecontent_validate (datatype type, void *data);

