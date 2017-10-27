#ifndef SEMA_H
#define SEMA_H
#include <pthread.h>

typedef struct sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int v;
} sem_t;

void sem_init(sem_t*, int);

void sem_wait(sem_t*);

void sem_post(sem_t*);

#endif // SEMA_H
