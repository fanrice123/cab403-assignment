#ifndef THRDPOOL_H
#define THRD_OOL_H
#include <stddef.h>
#include <stdbool.h>
#include <signal.h>

extern volatile sig_atomic_t thrd_pool_suspend;

typedef struct thrd_pool thrd_pool_t;

bool add_task(struct thrd_pool *pool, void (*func)(void*), void *arg, size_t arg_size);

thrd_pool_t *pool_create(int thread_num);

void pool_destroy(struct thrd_pool *pool);

#endif // THRDPOOL_H
