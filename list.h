#ifndef LIST_H
#define LIST_H
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

struct list_node {
    void *data;
    struct list_node *next;
};

struct list {
    struct list_node *head;
    int (*compare)(void*, void*);
    size_t size;
};



struct list *list_create(int (*comparer)(void*, void*));

void list_destroy(struct list*);

bool list_add(struct list*, void *data);

bool list_set(struct list*, void *old_item, void *new_item, int (*cmp)(void*, void*));

void *list_find(struct list*, void *item, int (*cmp)(void*,void*));

void list_clear(struct list*);
#endif // LIST_H
