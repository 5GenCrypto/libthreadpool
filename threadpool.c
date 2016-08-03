#include "threadpool.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void* threadpool_worker (void* arg);
static int job_available (threadpool *pool);

threadpool* threadpool_create (size_t nthreads)
{
    threadpool *pool = malloc(sizeof(threadpool));
    assert(pool != NULL);

    pool->num_active_jobs = 0;
    pool->first_job = NULL;
    pool->last_job  = NULL;

    pool->job_list_lock = malloc(sizeof(pthread_mutex_t));
    assert(pool->job_list_lock != NULL);
    pthread_mutex_init(pool->job_list_lock, NULL);

    pool->worker_wakeup_lock = malloc(sizeof(pthread_mutex_t));
    assert(pool->worker_wakeup_lock != NULL);
    pthread_mutex_init(pool->worker_wakeup_lock, NULL);

    pool->worker_wakeup_cond = malloc(sizeof(pthread_cond_t));
    assert(pool->worker_wakeup_cond != NULL);
    pthread_cond_init(pool->worker_wakeup_cond, NULL);

    pool->flag_exit_please = 0;

    pool->nthreads = nthreads;
    pool->threads  = malloc(nthreads * sizeof(pthread_t));
    assert(pool->threads != NULL);

    for (size_t i = 0; i < nthreads; i++) {
        int err = pthread_create(&pool->threads[i], NULL, threadpool_worker, pool);
        assert(err == 0);
    }

    return pool;
}

void threadpool_destroy (threadpool *pool)
{
    pool->flag_exit_please = 1;
    pthread_cond_broadcast(pool->worker_wakeup_cond);

    for (size_t i = 0; i < pool->nthreads; i++) {
        int err = pthread_join(pool->threads[i], NULL);
        assert(err == 0);
    }
    assert(pool->num_active_jobs == 0); // we should be done with all jobs now
    free(pool->threads);

    assert(pool->first_job == NULL); // there should be no jobs left in the queue
    assert(pool->last_job == NULL);

    pthread_mutex_destroy(pool->job_list_lock);
    free(pool->job_list_lock);

    pthread_mutex_destroy(pool->worker_wakeup_lock);
    free(pool->worker_wakeup_lock);

    pthread_cond_destroy(pool->worker_wakeup_cond);
    free(pool->worker_wakeup_cond);

    free(pool);
}

void threadpool_add_job (threadpool *pool, void (*func)(void*), void* arg)
{
    job_list *new = malloc(sizeof(job_list));
    assert(new != NULL);
    new->func = func;
    new->arg  = arg;
    new->next = NULL;

    pthread_mutex_lock(pool->job_list_lock);
    if (pool->first_job == NULL) {
        pool->first_job = new;
        pool->last_job  = new;
    } else {
        job_list *tmp  = pool->last_job;
        pool->last_job = new;
        tmp->next      = new;
    }
    pool->num_active_jobs += 1;
    pthread_mutex_unlock(pool->job_list_lock);
    pthread_cond_signal(pool->worker_wakeup_cond);
}

void threadpool_wait (threadpool *pool)
{
    while (pool->num_active_jobs > 0) {
        sleep(0.1);
    }
}

////////////////////////////////////////////////////////////////////////////////

static void* threadpool_worker (void* arg)
{
    threadpool *pool = arg;
    while (1) {
        // if no job is in the queue and exit has been requested, exit
        if (!job_available(pool) && pool->flag_exit_please) {
            pthread_exit(NULL);
        }

        // if no job is available, wait
        if (!job_available(pool)) {
            if (pool->flag_exit_please)
                continue;
            pthread_mutex_lock(pool->worker_wakeup_lock);
            while (!(job_available(pool) || pool->flag_exit_please)) {
                pthread_cond_wait(pool->worker_wakeup_cond, pool->worker_wakeup_lock);
            }
            pthread_mutex_unlock(pool->worker_wakeup_lock);
            continue;
        }

        // otherwise, get a job
        pthread_mutex_lock(pool->job_list_lock);
        if (!job_available(pool)) {
            pthread_mutex_unlock(pool->job_list_lock);
            continue;
        }
        job_list *node = pool->first_job;
        pool->first_job = node->next;
        if (pool->first_job == NULL) {
            pool->last_job = NULL;
        }
        pthread_mutex_unlock(pool->job_list_lock);

        // do the job
        node->func(node->arg);
        free(node);

        // since we finished, fix the count
        pthread_mutex_lock(pool->job_list_lock);
        pool->num_active_jobs -= 1;
        pthread_mutex_unlock(pool->job_list_lock);
    }
}

static int job_available (threadpool *pool)
{
    return pool->first_job != NULL;
}
