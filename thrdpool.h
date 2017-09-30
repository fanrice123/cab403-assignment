#ifndef THRDPOOL_H
#define THRD_OOL_H
#include <stdbool.h>
#include <stdatomic.h>

typedef struct thrd_pool thrd_pool_t;

bool add_task(struct thrd_pool *pool, void (*func)(void*), void *arg);

thrd_pool_t *pool_create(int thread_num);

void pool_destroy(struct thrd_pool *pool);

#endif // THRDPOOL_H
