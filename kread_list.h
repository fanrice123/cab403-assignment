#ifndef KREAD_LIST_H
#define KREAD_LIST_H
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include "sema.h"

struct kread_list {
    void **data;
    size_t size;
    size_t capacity;
    sem_t resrc_sem;
    sem_t read_count_sem;
    sem_t queue_sem;
    size_t read_count;

};

struct kread_list *kread_list_create(void);

void kread_list_destroy(struct kread_list*);

bool kread_list_add(struct kread_list*, void *data);
void *kread_list_get(struct kread_list*, size_t index);

// return old data
void *kread_list_set(struct kread_list*, size_t index, void *data);
size_t kread_list_find(struct kread_list*, void *data, bool (*cmp)(void*, void*));
#endif // KREAD_LIST_H
