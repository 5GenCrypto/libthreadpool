#ifndef __BRENT_THREADPOOL__
#define __BRENT_THREADPOOL__

#include <pthread.h>

typedef unsigned long ul;

typedef struct job_list_node {
    void (*func) (void*);
    void* arg;
    struct job_list_node *next;
} job_list_node;

typedef struct {
    job_list_node *first;
    job_list_node *last;
    pthread_mutex_t *lock;
    pthread_mutex_t *wakeup_lock;
    pthread_cond_t *wakeup_cond;
    int job_available;
    int exit_please;
} job_list;

typedef struct {
    pthread_t *threads;
    ul nthreads;
    job_list *jobs;
} threadpool;

#define THREADPOOL_NCORES (sysconf(_SC_NPROCESSORS_ONLN))

threadpool* threadpool_create (size_t nthreads);
void threadpool_destroy (threadpool *pool);
void threadpool_add_job (threadpool *pool, void (*func)(void*), void* arg);

#endif
