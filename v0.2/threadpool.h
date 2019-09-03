

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <pthread.h>

typedef enum 
{
	immediate_shutdown = 1,
	graceful_shutdown = 2
}threadpool_shutdown_t;


typedef struct
{
	void (*function)(void *);
	void *argument;
}threadpool_task_t;




struct threadpool_t
{
	pthread_mutex_t lock;
	pthread_cond_t notify;
	pthread_t *threads;
	threadpool_task_t *queue;
	int thread_count;
	int queue_size;
	int head;
	int tail;
	int count;
	int shutdown;
	int started;
};


threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);
int threadpool_add(threadpool_t *pool, void(*function)(void *), void *argument, int flags);
int threadpool_destroy(threadpool_t *pool, int flags);
int threadpool_free(threadpool_t *pool);
static void *threadpool_thread(void *threadpool);


#endif
