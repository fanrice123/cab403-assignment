#include "thrdpool.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <time.h>
#include <pthread.h>

#ifndef NDEBUG
#include <stdio.h>
#endif // NDEBUG
#define LOCK_WRAP(MUTEXP,X) pthread_mutex_lock(MUTEXP); \
                            X; \
                            pthread_mutex_unlock(MUTEXP) // without semicolon

typedef struct job {
    void (*func)(void*);
    void *arg;
    struct job *next;
} job_t;

typedef struct job_queue {
    job_t *head;
    job_t *tail;
    pthread_mutex_t mutex;
    atomic_int len;
} job_queue_t;

typedef struct thrd {
    int id;
    pthread_t impl;
    struct thrd_pool* pool;
} thrd_t;

typedef struct semaphore {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int v;
} sem_t;

struct thrd_pool {
    int thread_num;
    thrd_t* threads;
    atomic_int working;
    pthread_mutex_t iw;
    int idle_waiting;
    job_queue_t jobq;
    sem_t has_job;
    volatile atomic_bool alive;
};

static void job_queue_init(job_queue_t* jq);
static void job_enqueue(job_queue_t *jq, job_t *new_job);
static job_t *job_dequeue(job_queue_t *jq);
static void job_queue_clear(job_queue_t *jq);

static void thrd_init(thrd_t *thread, struct thrd_pool *pool, int t_id);
static void thrd_exec(thrd_t *thread);

static void sem_init(sem_t *sem, int v);
static void sem_post(sem_t *sem);
static void sem_post_all(sem_t *sem);
static void sem_wait(sem_t *sem);




    

bool add_task(struct thrd_pool *pool, void (*func)(void*), void* arg) {
    job_t* new_job = (job_t*) malloc(sizeof(job_t));
    job_queue_t *jq = &pool->jobq;

    if (!new_job)
        return false;

    new_job->func = func;
    new_job->arg = arg;

#ifndef NDEBUG
    puts("Enter function: job_enqueue");
#endif // NDEBUG
    job_enqueue(&pool->jobq, new_job);
    sem_post(&pool->has_job);
#ifndef NDEBUG
    puts("Signaled for new task.");
#endif // NDEBUG

    return true;
}

struct thrd_pool *pool_create(int thread_num) {
    struct thrd_pool *pool = (struct thrd_pool*) malloc(sizeof(struct thrd_pool));

    if (!pool)
        return pool;

    pool->thread_num = pool->working = 0;
    pool->alive = true;
    sem_init(&pool->has_job, false);

    job_queue_init(&pool->jobq);

#ifndef NDEBUG
    puts("job queue initialized.");
#endif

    pool->threads = (thrd_t*) malloc(sizeof(thrd_t) * thread_num);
    if (!pool->threads) {
        free(pool);
        return NULL;
    }
#ifndef NDEBUG
    puts("thread list initialized.");
#endif
    

    for (int i = 0; i != thread_num; ++i) {
        thrd_init(pool->threads + i, pool, i);
        ++pool->thread_num;
    }

    return pool;
}

void pool_destroy(struct thrd_pool *pool) {
#ifndef NDEBUG
    puts("Start destroying thread pool...");
#endif // NDEBUG
    if (!pool)
        return;

    pool->alive = false;
#ifndef NDEBUG
    puts("Start cleaning idle threads.");
#endif // NDEBUG

    pthread_mutex_lock(&pool->iw);
    while (pool->idle_waiting != 0) {
#ifndef NDEBUG
        puts("sem posted.");
#endif // NDEBUG
        sem_post(&pool->has_job);
        --pool->idle_waiting;
    }
    pthread_mutex_unlock(&pool->iw);
    thrd_t *thrds = pool->threads;
    for (int i = 0; i != pool->thread_num; ++i) {
        pthread_join(thrds[i].impl, NULL);
#ifndef NDEBUG
        printf("thread %d has joined.\n", i);
#endif // NDEBUG
    }
    

#ifndef NDEBUG
    puts("Idle threads perished.");
#endif // NDEBUG

    job_queue_clear(&pool->jobq);

    free(pool->threads);
    free(pool);
#ifndef NDEBUG
    puts("Thread pool destroyed.");
#endif // NDEBUG
}

static void job_queue_init(job_queue_t* jq) {
    jq->head = jq->tail = NULL;
    jq->len = 0;
}

static void job_enqueue(job_queue_t *jq, job_t *new_job) {
    new_job->next = NULL;

    pthread_mutex_lock(&jq->mutex);
    if (jq->len != 0) {
        jq->tail->next = new_job;
        jq->tail = new_job;
    } else {
        jq->head = jq->tail = new_job;
    }
    ++jq->len;
    pthread_mutex_unlock(&jq->mutex);
}

static job_t *job_dequeue(job_queue_t *jq) {
    pthread_mutex_lock(&jq->mutex);
    job_t *new_job = jq->head;
    jq->head = new_job->next;
    --jq->len;
    pthread_mutex_unlock(&jq->mutex);

    return new_job;
}

static void job_queue_clear(job_queue_t *jq) {
#ifndef NDEBUG
    puts("Start destroying job_queue...");
#endif // NDEBUG
    job_t *curr = jq->head;

    pthread_mutex_lock(&jq->mutex);
    while (curr) {
        job_t *next = curr->next;
        free(curr);
        curr = next;
    }
    jq->len = 0;
    pthread_mutex_unlock(&jq->mutex);

#ifndef NDEBUG
    puts("job_queue destroyed.");
#endif // NDEBUG
}

static void thrd_init(thrd_t *thread, struct thrd_pool *pool, int t_id) {
    thread->id = t_id;
    thread->pool = pool;
    
    pthread_create(&thread->impl, NULL, (void*) thrd_exec, thread);
}

static void thrd_exec(thrd_t *thread) {
#ifndef NDEBUG
    printf("thread %d is started.\n", thread->id);
#endif // NDEBUG
    
    struct thrd_pool *pool = thread->pool;
    while (pool->alive) {

#ifndef NDEBUG
        printf("thread %d waiting new job.\n", thread->id);
#endif // NDEBUG

        LOCK_WRAP(&pool->iw, ++pool->idle_waiting);
        sem_wait(&pool->has_job);
        LOCK_WRAP(&pool->iw, --pool->idle_waiting);
#ifndef NDEBUG
        printf("thread %d quited waiting.\n", thread->id);
#endif // NDEBUG
        if (pool->alive) {

#ifndef NDEBUG
            printf("thread %d getting new job\n", thread->id);
#endif // NDEBUG
            job_t *new_job = job_dequeue(&pool->jobq);
#ifndef NDEBUG
            printf("thread %d got new job\n", thread->id);
#endif // NDEBUG
            ++pool->working;
            void (*func)(void*) = new_job->func;
            void* arg = new_job->arg;
            func(arg);
#ifndef NDEBUG
            printf("thread %d finished job\n", thread->id);
#endif // NDEBUG
            free(new_job);

            --pool->working;
        }
    }
#ifndef NDEBUG
    printf("thread %d is quiting thrd_exec...\n", thread->id);
#endif // NDEBUG
}

static void sem_init(sem_t *sem, int v) {
    sem->v = v;
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
}

static void sem_post(sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    ++sem->v;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
}

static void sem_wait(sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    while (sem->v == 0)
        pthread_cond_wait(&sem->cond, &sem->mutex);
    --sem->v;
    pthread_mutex_unlock(&sem->mutex);
}
