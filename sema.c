#include "sema.h"
#include <pthread.h>

void sem_init(sem_t *sem, int v) {
    sem->v = v;
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
}

void sem_wait(sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    while (sem->v == 0)
        pthread_cond_wait(&sem->cond, &sem->mutex);
    --sem->v;
    pthread_mutex_unlock(&sem->mutex);
}

void sem_post(sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    ++sem->v;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
}
