#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#define NLIST_CAPACITY 10u

static struct list_node *do_find(struct list *l, void *item, int (*cmp)(void*, void*));

struct list *list_create(int (*comparer)(void*, void*)) {
    struct list *new_list = malloc(sizeof(struct list));

    new_list->head = NULL;
    new_list->compare = comparer;
    new_list->size = 0u;

    return new_list;
}

void list_destroy(struct list *l) {
    struct list_node *curr = l->head;

    while (curr) {
        struct list_node *next = curr->next;
        free(curr);
        curr = next;
    }
    free(l);
}

bool list_add(struct list *l, void *item) {

    struct list_node *new_node = malloc(sizeof(struct list_node));
    if (!new_node)
        return false;

    new_node->data = item;
    new_node->next = NULL;

    struct list_node **curr = &l->head;
    while (*curr && l->compare(new_node, *curr) > 0)
        curr = &(*curr)->next;
    new_node->next = *curr;
    *curr = new_node;
    ++l->size;

    return true;
}

bool list_set(struct list *l, void *old_item, void *new_item, int (*cmp)(void*, void*)) {
    bool ret_val = false;

    struct list_node *found = do_find(l, old_item, cmp);
    if (found) {
        found->data = new_item;
        ret_val = true;
    }

    return ret_val;
}

// return list->size if not found.
void *list_find(struct list *l, void *item, int (*cmp)(void*, void*)) {
    void *found = NULL;

    struct list_node *node = do_find(l, item, cmp);

    if (node)
        found = node->data;

    return found;
}

void list_clear(struct list *l) {
    
    struct list_node *curr = l->head; 

    while (curr) {
        struct list_node *next = curr->next;
        free(curr->data);
        free(curr);
        curr = next;
    }

    l->head = NULL;
    l->size = 0;
}

static struct list_node *do_find(struct list *l, void *item, int (*cmp)(void*, void*)) {
    struct list_node *ret_val = NULL;

    for (struct list_node *curr = l->head; curr; curr = curr->next) {
        if (cmp(item, curr->data) == 0) {
            ret_val = curr;
            break;
        }
    }

    return ret_val;
}
