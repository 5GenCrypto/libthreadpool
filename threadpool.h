#ifndef __BRENT_THREADPOOL__
#define __BRENT_THREADPOOL__

#include <pthread.h>
#include <unistd.h>

#define THREADPOOL_OK    0
#define THREADPOOL_ERR (-1)

typedef struct threadpool threadpool;

threadpool * threadpool_create(size_t nthreads);
void threadpool_destroy(threadpool *pool);
int threadpool_add_job(threadpool *pool, void (*func)(void *), void *arg);
void threadpool_wait(threadpool *pool);

#endif
