#include "list.h"
#include "heap.h"
#include "logger.h"

list * list_new () {
	list *new = calloc (1, sizeof (list));

	check_mem(new);

	if (!new) {
		return NULL;
	}

	new->start = new->end = NULL;
	new->elements = 0;
	new->isSorted = FALSE;
	new->add = &list_add;
	new->remove = &list_remove;
	new->find = &list_find;
	new->sort = &list_sort;
	new->reverse = &list_reverse;
	new->isCircular = &list_isCircular;
	new->isValid = &list_isValid;

	return new;
}

status list_add (list *this, record *rec) {
	node *new = calloc (1, sizeof (node));

	check_mem(new);

	if (!new) {
		return STATUS_MEMORY_ERROR;
	}

	new->key = rec->getKey();
	new->rec = rec;

	if (this->elements == 0) {
		/* empty list */
		assert (this->start == NULL && this->end == NULL);
		this->start = new;
		new->next = this->end;
	}
	else {
		/* TODO: if rec->cmp() != null use cmp to create sorted list */
		node *curr = this->start;
		this->start = new;
		new->next = curr;
	}

	this->elements++;

    return STATUS_OK;
}

status list_remove (list *this, record *rec) {
    return STATUS_OK;
}

void list_sort (list *this) {
    return;
}

void list_reverse (list *this) {
}

const heap * list_asHeap(list *this) {
    /* heap_heapify */
    return NULL;
}

/** check if a list is circular */
bool list_checkIsCircular (list *this) {
	bool ret = FALSE;
	node *slow = NULL, *fast = NULL;

	if (!this->start) {
		return FALSE;
	}

	*slow = this->start;
	*fast = this->start->next;

	while (TRUE) {
		if (!fast || !fast->next) {
			/* NULL end point, not circular */
			break;
		}
		else if (fast == slow || fast->next == slow) {
			/* cycle */
			ret = TRUE;
			break;
		}
		slow = slow->next;
		fast = fast->next->next;
	}

	return ret;
}

/** verify internal consistency, eg. check for that num elements do not overrun
 *  unless it's circular
 */
bool list_isValid (list *this) {
	bool ret = TRUE;
	node *curr = NULL;
	int c = 0;

	curr = this->start;

	if (!curr) {
		assert (this->elements == 0);
		return FALSE;
	}

	while (curr != NULL) {
		curr = curr->next;
		c++;

		if (c >= this->elements) {
			/* have gone past start if more than elements than added */
			if (!this->isCircular(this)) {
				assert (!list_checkCircular(this));
			}
			else {
				ret = FALSE;
			}
			break;
		}
	}

	return ret;
}

bool list_isCircular (list *this) {
	return this->start == this->end;
}

status list_destroy (list * this) {
	/* iterate through list, free nodes, records, etc
	 * reset list
	 */
	return STATUS_OK;
}
