#ifndef __BRENT_THREADPOOL__
#define __BRENT_THREADPOOL__

#include <pthread.h>

#define NCORES (sysconf(_SC_NPROCESSORS_ONLN))

typedef struct job_list {
    void (*func) (void*);
    void* arg;
    struct job_list *next;
} job_list;

typedef struct {
    size_t nthreads;
    pthread_t *threads;
    size_t num_active_jobs;
    job_list *first_job;
    job_list *last_job;
    pthread_mutex_t *job_list_lock;
    pthread_mutex_t *worker_wakeup_lock;
    pthread_cond_t  *worker_wakeup_cond;
    int flag_exit_please;
} threadpool;

threadpool* threadpool_create (size_t nthreads);
void threadpool_destroy (threadpool *pool);
void threadpool_add_job (threadpool *pool, void (*func)(void*), void* arg);
void threadpool_wait (threadpool *pool);

#endif
