#ifndef LIST_H
#define LIST_H

#include <stdbool.h>

struct SList {
	void *val;
	struct SList *nex;
};

void slist_append(struct SList **head, void *val);

void slist_remove(struct SList **head, struct SList **item);

void slist_remove_all(struct SList **head, void *val);

struct SList *slist_find(struct SList **head, bool (*test)(const void *val, const void *data), const void *data);

long slist_length(struct SList *head);

struct SList *slist_shallow_clone(struct SList *head);

void slist_free(struct SList **head);

bool slist_test_strcasecmp(const void *value, const void *data);

#endif // LIST_H

