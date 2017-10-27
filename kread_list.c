#include "kread_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#define NLIST_CAPACITY 10u
#define lock(MUTEX) pthread_mutex_lock(MUTEX)
#define unlock(MUTEX) pthread_mutex_unlock(MUTEX)


static bool increase_capacity(struct kread_list*);
    
struct kread_list *kread_list_create(void) {
    struct kread_list *new_kread_list = malloc(sizeof(struct kread_list));

    void **alloc = malloc(sizeof(void*) * NLIST_CAPACITY);
    new_kread_list->data = alloc;
    new_kread_list->size = 0u;
    new_kread_list->capacity = NLIST_CAPACITY;
    new_kread_list->read_count = 0u;

    sem_init(&new_kread_list->resrc_sem, 1);
    sem_init(&new_kread_list->read_count_sem, 1);
    sem_init(&new_kread_list->queue_sem, 1);

    return new_kread_list;
}

void kread_list_destroy(struct kread_list *l) {
    free(l->data);
    free(l);
}

bool kread_list_add(struct kread_list* l, void *item) {
    bool ret_val;

    sem_wait(&l->queue_sem);
    sem_wait(&l->resrc_sem);
    sem_post(&l->queue_sem);

    // write data...

    if (l->capacity == l->size && !increase_capacity(l)) {
        ret_val = false;
    } else {
        l->data[l->size++] = item;
        ret_val = true;
    }

    sem_post(&l->resrc_sem);

    return ret_val;
}

void *kread_list_get(struct kread_list* l, size_t index) {
    void *ret_val;

    sem_wait(&l->queue_sem);
    sem_wait(&l->read_count_sem);
    if (l->read_count == 0)
        sem_wait(&l->resrc_sem);
    ++l->read_count;
    sem_post(&l->queue_sem);
    sem_post(&l->read_count_sem);

    // read...
    if (index >= l->size)
        ret_val = NULL;
    else
        ret_val = l->data[index];

    sem_wait(&l->read_count_sem);
    --l->read_count;
    if (l->read_count == 0)
        sem_post(&l->resrc_sem);

    sem_post(&l->read_count_sem);

    return ret_val;
}

void *kread_list_set(struct kread_list *l, size_t index, void *item) {
    void *old_item;

    sem_wait(&l->queue_sem);
    sem_wait(&l->resrc_sem);
    sem_post(&l->queue_sem);


    // write...
    if (index >= l->size) {
        old_item = NULL;
    } else {
        old_item = l->data[index];
        l->data[index] = item;
    }

    sem_post(&l->resrc_sem);

    return old_item;
}

// return kread_list->size if not found.
size_t kread_list_find(struct kread_list *l, void *item, bool (*cmp)(void*, void*)) {
    void *ret_val;

    sem_wait(&l->queue_sem);
    sem_wait(&l->read_count_sem);
    if (l->read_count == 0)
        sem_wait(&l->resrc_sem);
    ++l->read_count;
    sem_post(&l->queue_sem);
    sem_post(&l->read_count_sem);

    // read...

    size_t i = 0u;
    for (; i != l->size; ++i) {
        if (cmp(item, l->data[i]))
            break;
    }

    sem_wait(&l->read_count_sem);
    --l->read_count;
    if (l->read_count == 0)
        sem_post(&l->resrc_sem);

    sem_post(&l->read_count_sem);

    return i;
}

static bool increase_capacity(struct kread_list *l) {

    size_t new_capacity = l->capacity * 3;
    void **new_arr = malloc(new_capacity * sizeof(void*));
    if (!new_arr)
        return false;
    size_t size = l->size;
    memcpy(new_arr, l->data, size * sizeof(void*));
    free(l->data);
    l->data = new_arr;
    l->capacity = new_capacity;

    return true;
}
