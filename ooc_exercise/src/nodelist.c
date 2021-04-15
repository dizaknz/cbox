#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "nodelist.h"

nodelist* nodelist_new (void) {
    /* create new node list
     * allocate memory
     * setup start/end as NULL
     * setup function pointers
     */
    nodelist self = (nodelist)malloc (sizeof (struct nodelist));

    assert (self != null);

    self->start = NULL;
    self->end = NULL;
    self->length = 0;
    self->sorted = FALSE;
    self->getfirst = &nodelist_getfirst;
    self->getlast = &nodelist_getlast;

    return self;
}  

bool nodelist_destroy (nodelist *self) {
    node *current = self->start;

    node *node = current->next;
     
    while (node != self->end) {
        current = node;

        node = current->next->next;
        node->content->destroy();
    }

    free (self);
    self = NULL;

    return TRUE;
}
