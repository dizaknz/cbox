#include "node/nodecontent.h"

typedef unsigned char bool;
typedef TRUE (unsigned char)1;
typedef FALSE (unsigned char)0;
typedef struct node node;

struct node {
    int index;
    nodecontent *content;
    node *next;
};

typedef struct nodelist nodelist;

struct nodelist {
    int length;
    bool sorted;
    node *start;
    node *end;

    node* (*getfirst)(const nodelist *self);
    node* (*getlast)(const nodelist *self);
    bool (*insert)(const nodecontent *content);
    bool (*insertAt)(const nodecontent *content, const node *node);
    bool (*append)(const nodecontent *content);
    bool (*remove)(const node *node);
    bool (*removeAll)(void);
    void (*print)(void);
};

/* methods */
node * nodelist_getfirst (const nodelist *self);
node * nodelist_getlast (const nodelist *self);
bool nodelist_insert (const nodecontent *content);
bool nodelist_insertAt (const nodecontent *content, const node *node);
bool nodelist_append (const nodecontent *content);
bool nodelist_remove (const node *node);
bool nodelist_removeAll (void);
void nodelist_print (void);

/* constructor */
nodelist* nodelist_new (void);

/* destructor */
bool nodelist_destroy (nodelist *self);

