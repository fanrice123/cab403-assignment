#include "thrdpool.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#include <signal.h>
#include <time.h>
#include "sema.h"
#include <pthread.h>
#include <stdio.h>

#define LOCK_WRAP(MUTEXP,X) do { \
                                pthread_mutex_lock(MUTEXP); \
                                X; \
                                pthread_mutex_unlock(MUTEXP); \
                            } while(0)
#ifndef NDEBUG
#define DEBUG_MSG(STR) do { \
                           fputs(STR, stderr); \
                       } while(0)
#define DEBUG_PRINTF(...) do { \
                               fprintf(stderr, __VA_ARGS__); \
                           } while(0)
#else
#define DEBUG_MSG(STR)
#define DEBUG_PRINTF(...)
#endif


volatile sig_atomic_t thrd_pool_suspend = 0;

typedef struct job {
    void (*func)(void*);
    void *arg;
    size_t arg_size;
    struct job *next;
} job_t;

typedef struct job_queue {
    job_t *head;
    job_t *tail;
    pthread_mutex_t mutex;
    int len;
} job_queue_t;

typedef struct thrd {
    int id;
    pthread_t impl;
    struct thrd_pool* pool;
} thrd_t;

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



    

bool add_task(struct thrd_pool *pool, void (*func)(void*), void *arg, size_t arg_size) {

    if (thrd_pool_suspend)
        return false;

    job_t* new_job = malloc(sizeof(job_t));
    job_queue_t *jq = &pool->jobq;

    void *private_arg = malloc(arg_size);
    memcpy(private_arg, arg, arg_size);

    if (!new_job)
        return false;

    new_job->func = func;
    new_job->arg = private_arg;
    new_job->arg_size = arg_size;

    DEBUG_MSG("Enter function: job_enqueue\n");

    job_enqueue(&pool->jobq, new_job);
    sem_post(&pool->has_job);

    DEBUG_MSG("Signaled for new task.\n");

    return true;
}

struct thrd_pool *pool_create(int thread_num) {
    struct thrd_pool *pool = (struct thrd_pool*) malloc(sizeof(struct thrd_pool));

    if (!pool)
        return pool;

    pool->thread_num = pool->working = 0;
    pool->alive = true;
    sem_init(&pool->has_job, 0);

    job_queue_init(&pool->jobq);

    DEBUG_MSG("job queue initialized.\n");

    pool->threads = (thrd_t*) malloc(sizeof(thrd_t) * thread_num);
    if (!pool->threads) {
        free(pool);
        return NULL;
    }

    DEBUG_MSG("thread list initialized.\n");

    for (int i = 0; i != thread_num; ++i) {
        thrd_init(pool->threads + i, pool, i);
        ++pool->thread_num;
    }

    return pool;
}

void pool_destroy(struct thrd_pool *pool) {
    DEBUG_MSG("Start destroying thread pool...\n");

    if (!pool)
        return;

    pool->alive = false;
    DEBUG_MSG("Start cleaning idle threads.\n");

    pthread_mutex_lock(&pool->iw);
    while (pool->idle_waiting != 0) {
        sem_post(&pool->has_job);
        --pool->idle_waiting;
    }
    pthread_mutex_unlock(&pool->iw);
    thrd_t *thrds = pool->threads;
    for (int i = 0; i != pool->thread_num; ++i) {
        pthread_kill(thrds[i].impl, SIGUSR1);
        pthread_join(thrds[i].impl, NULL);
        DEBUG_PRINTF("thread %d has joined.\n", i);
    }
    

    DEBUG_MSG("Idle threads perished.\n");

    job_queue_clear(&pool->jobq);

    free(pool->threads);
    free(pool);
    DEBUG_MSG("Thread pool destroyed.\n");
}

static void job_queue_init(job_queue_t* jq) {
    jq->head = jq->tail = NULL;
    jq->len = 0;
    pthread_mutex_init(&jq->mutex, 0);
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
    DEBUG_MSG("Start destroying job_queue...\n");
    job_t *curr = jq->head;

    pthread_mutex_lock(&jq->mutex);
    while (curr) {
        job_t *next = curr->next;
        free(curr);
        curr = next;
    }
    jq->len = 0;
    pthread_mutex_unlock(&jq->mutex);

    DEBUG_MSG("job_queue destroyed.\n");
}

static void thrd_init(thrd_t *thread, struct thrd_pool *pool, int t_id) {
    thread->id = t_id;
    thread->pool = pool;
    
    pthread_create(&thread->impl, NULL, (void*) thrd_exec, thread);
}

static void thrd_exec(thrd_t *thread) {
    // Block SIGINT

    sigset_t mask;
    sigfillset(&mask); // mask all signals
    /*
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    */
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL)) {
        fprintf(stderr, "signal mask on thread %d failed.\n", thread->id);
        return;
    }

    DEBUG_PRINTF("signal mask on thread %d succeeded.\n", thread->id);


    DEBUG_PRINTF("thread %d is started.\n", thread->id);
    
    struct thrd_pool *pool = thread->pool;
    while (pool->alive && !thrd_pool_suspend) {

        DEBUG_PRINTF("thread %d waiting new job.\n", thread->id);

        LOCK_WRAP(&pool->iw, ++pool->idle_waiting);
        sem_wait(&pool->has_job);
        LOCK_WRAP(&pool->iw, --pool->idle_waiting);

        DEBUG_PRINTF("thread %d quited waiting.\n", thread->id);
        if (pool->alive) {

            DEBUG_PRINTF("thread %d getting new job\n", thread->id);
            job_t *new_job = job_dequeue(&pool->jobq);
            DEBUG_PRINTF("thread %d got new job\n", thread->id);
            ++pool->working;
            void (*func)(void*) = new_job->func;
            void* arg = new_job->arg;
            func(arg);
            DEBUG_PRINTF("thread %d finished job\n", thread->id);
            free(arg);
            free(new_job);

            --pool->working;
        }
    }
    DEBUG_PRINTF("thread %d is quiting thrd_exec...\n", thread->id);
}

