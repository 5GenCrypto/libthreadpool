#include "threadpool.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct job_t {
    void (*func)(void *);
    void *arg;
    struct job_t *next;
} job_t;

typedef struct {
    threadpool *pool;
    size_t idx;
} args_t;

struct threadpool {
    size_t nthreads;
    pthread_t *threads;
    job_t *first, *last;
    pthread_mutex_t job_lock;
    pthread_mutex_t *wakeup_locks;
    args_t *args;
    pthread_cond_t  wakeup_cond;
    atomic_bool flag_exit_please;
};

static void * threadpool_worker(void *arg);

threadpool *
threadpool_create(size_t nthreads)
{
    threadpool *pool;

    if ((pool = calloc(1, sizeof pool[0])) == NULL)
        return NULL;
    pool->nthreads = nthreads;
    if ((pool->threads = calloc(nthreads, sizeof pool->threads[0])) == NULL)
        goto error;

    atomic_init(&pool->flag_exit_please, false);
    pool->first = NULL;
    pool->last = NULL;

    pthread_mutex_init(&pool->job_lock, NULL);
    pthread_cond_init(&pool->wakeup_cond, NULL);
    pool->wakeup_locks = calloc(nthreads, sizeof pool->wakeup_locks[0]);
    pool->args = calloc(nthreads, sizeof pool->args[0]);
    for (size_t i = 0; i < nthreads; i++) {
        pool->args[i].pool = pool;
        pool->args[i].idx = i;
        pthread_mutex_init(&pool->wakeup_locks[i], NULL);
        (void) pthread_create(&pool->threads[i], NULL, threadpool_worker, &pool->args[i]);
    }
    return pool;
error:
    if (pool)
        free(pool);
    return NULL;
}

void
threadpool_destroy(threadpool *pool)
{
    if (pool == NULL) return;
    atomic_store(&pool->flag_exit_please, true);
    pthread_cond_broadcast(&pool->wakeup_cond);
    for (size_t i = 0; i < pool->nthreads; i++)
        (void) pthread_join(pool->threads[i], NULL);
    free(pool->threads);

    pthread_mutex_destroy(&pool->job_lock);
    for (size_t i = 0; i < pool->nthreads; i++)
        pthread_mutex_destroy(&pool->wakeup_locks[i]);
    free(pool->wakeup_locks);
    free(pool->args);
    pthread_cond_destroy(&pool->wakeup_cond);

    free(pool);
}

int
threadpool_add_job(threadpool *pool, void (*func)(void *), void *arg)
{
    job_t *new;

    if ((new = calloc(1, sizeof new[0])) == NULL)
        return THREADPOOL_ERR;
    new->func = func;
    new->arg  = arg;
    new->next = NULL;

    pthread_mutex_lock(&pool->job_lock);
    {
        if (pool->first) {
            job_t *tmp = pool->last;
            pool->last = tmp->next = new;
        } else {
            pool->first = pool->last = new;
        }
    }
    pthread_mutex_unlock(&pool->job_lock);
    pthread_cond_signal(&pool->wakeup_cond);
    return THREADPOOL_OK;
}

static void *
threadpool_worker(void *args_)
{
    const struct timespec t = { .tv_sec = 0, .tv_nsec = 100000L };

    args_t *args = args_;
    threadpool *pool = args->pool;
    size_t idx = args->idx;
    while (1) {
        if (pool->first) {
            job_t *node = NULL;
            pthread_mutex_lock(&pool->job_lock);
            if (pool->first) {
                node = pool->first;
                pool->first = node->next;
                if (pool->first == NULL)
                    pool->last = NULL;
            }
            pthread_mutex_unlock(&pool->job_lock);

            if (node) {
                node->func(node->arg);
                free(node);
            }
        } else {
            if (atomic_load(&pool->flag_exit_please))
                pthread_exit(NULL);
            pthread_mutex_lock(&pool->wakeup_locks[idx]);
            pthread_cond_timedwait(&pool->wakeup_cond,
                                   &pool->wakeup_locks[idx], &t);
            pthread_mutex_unlock(&pool->wakeup_locks[idx]);
        }
    }
}
