#include "threadpool.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

static job_list* job_list_create (void);
static void job_list_destroy (job_list *jobs);
static int job_list_next_job (job_list *jobs);
static void job_list_add_job (job_list *jobs, void (*func)(void*), void* arg);
static void* threadpool_worker (void* arg);

static job_list* job_list_create (void)
{
    job_list *jobs = malloc(sizeof(job_list));
    assert(jobs != NULL);
    jobs->first = NULL;
    jobs->last  = NULL;

    jobs->lock = malloc(sizeof(pthread_mutex_t));
    assert(jobs->lock != NULL);
    pthread_mutex_init(jobs->lock, NULL);

    jobs->wakeup_lock = malloc(sizeof(pthread_mutex_t));
    assert(jobs->wakeup_lock != NULL);
    pthread_mutex_init(jobs->wakeup_lock, NULL);

    jobs->wakeup_cond = malloc(sizeof(pthread_cond_t));
    assert(jobs->wakeup_cond != NULL);
    pthread_cond_init(jobs->wakeup_cond, NULL);

    jobs->exit_please = 0;
    jobs->job_available = 0;

    return jobs;
}

static void job_list_destroy(job_list *jobs)
{
    job_list_node *cur = jobs->first;
    while (cur != NULL) {
        job_list_node *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    pthread_mutex_destroy(jobs->lock);
    free(jobs->lock);
    pthread_mutex_destroy(jobs->wakeup_lock);
    free(jobs->wakeup_lock);
    pthread_cond_destroy(jobs->wakeup_cond);
    free(jobs->wakeup_cond);
    free(jobs);
}

#define JOB_LIST_JOB_GIVEN 0
#define JOB_LIST_JOB_WAIT  1
#define JOB_LIST_JOB_EXIT  2

static int job_list_next_job (job_list *jobs)
{
    if (jobs->exit_please && jobs->first == NULL) {
        return JOB_LIST_JOB_EXIT;
    }

    if (jobs->first == NULL) {
        return JOB_LIST_JOB_WAIT;
    }

    pthread_mutex_lock(jobs->lock);
    job_list_node *node = jobs->first;
    if (node != NULL) {
        jobs->first = node->next;
        if (jobs->first == NULL) {
            jobs->last = NULL;
            jobs->job_available = 0;
        }
    }
    pthread_mutex_unlock(jobs->lock);

    // do the job
    if (node != NULL) {
        node->func(node->arg);
        free(node);
        return JOB_LIST_JOB_GIVEN;
    } else {
        return JOB_LIST_JOB_WAIT;
    }
}

static void job_list_add_job (job_list *jobs, void (*func)(void*), void* arg)
{
    job_list_node *new = malloc(sizeof(job_list_node));
    assert(new != NULL);
    new->func = func;
    new->arg  = arg;
    new->next = NULL;

    pthread_mutex_lock(jobs->lock);
    if (jobs->first == NULL) {
        jobs->first = new;
        jobs->last = new;
    } else {
        job_list_node *tmp = jobs->last;
        jobs->last = new;
        tmp->next = new;
    }

    jobs->job_available = 1;
    pthread_cond_signal(jobs->wakeup_cond);
    pthread_mutex_unlock(jobs->lock);
}

static void* threadpool_worker (void* arg)
{
    job_list *jobs = arg;
    while (1) {
        int msg = job_list_next_job(jobs);
        if (msg == JOB_LIST_JOB_GIVEN) {
            continue;
        }
        else if (msg == JOB_LIST_JOB_WAIT) {
            pthread_mutex_lock(jobs->wakeup_lock);
            while (!(jobs->job_available || jobs->exit_please))
                pthread_cond_wait(jobs->wakeup_cond, jobs->wakeup_lock);
            pthread_mutex_unlock(jobs->wakeup_lock);
            continue;
        }
        else if (msg == JOB_LIST_JOB_EXIT) {
            pthread_exit(NULL);
        }
    }
}

threadpool* threadpool_create (size_t nthreads)
{
    threadpool *pool = malloc(sizeof(threadpool));
    assert(pool != NULL);

    pool->nthreads = nthreads;
    pool->threads  = malloc(nthreads * sizeof(pthread_t));
    assert(pool->threads != NULL);

    pool->jobs = job_list_create();

    for (size_t i = 0; i < nthreads; i++) {
        int err = pthread_create(&pool->threads[i], NULL, threadpool_worker, pool->jobs);
        assert(err == 0);
    }

    return pool;
}

void threadpool_destroy (threadpool *pool)
{
    pool->jobs->exit_please = 1;
    pthread_cond_broadcast(pool->jobs->wakeup_cond);
    for (size_t i = 0; i < pool->nthreads; i++) {
        int err = pthread_join(pool->threads[i], NULL);
        assert(err == 0);
    }
    job_list_destroy(pool->jobs);
    free(pool->threads);
    free(pool);
}

void threadpool_add_job (threadpool *pool, void (*func)(void*), void* arg)
{
    job_list_add_job(pool->jobs, func, arg);
}
