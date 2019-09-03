
#include <cstdlib>
#include "threadpool.h"


// test
#include <iostream>


const int THREADPOOL_INVALID = -1;
const int THREADPOOL_LOCK_FAILURE = -2;
const int THREADPOOL_QUEUE_FULL = -3;
const int THREADPOOL_SHUTDOWN = -4;
const int THREADPOOL_THREAD_FAILURE = -5;
const int THREADPOOL_GRACEFUL = 1;

const int MAX_THREADS = 1024;
const int MAX_QUEUE = 65535;



threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{

	threadpool_t *pool;
	int i;

	do
	{
		if(thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
		{
			return NULL;
		}

		if((pool = (threadpool_t *)malloc(sizeof(threadpool_t)))  == NULL)
		{
			break;
		}

		// 初始化
		pool -> thread_count = 0;
		pool -> queue_size = queue_size;
		pool -> head = pool -> tail = pool -> count = 0;
		pool -> shutdown = pool -> started = 0;

		// 为线程池和任务队列分配内存空间
		pool -> threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
		pool -> queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);
		
		if((pthread_mutex_init(&(pool -> lock), NULL) != 0) || (pthread_cond_init(&(pool -> notify), NULL)) 
			|| (pool -> threads == NULL) || (pool -> queue == NULL))
		{
			break;
		}

		// 开始任务线程
		for(i = 0; i < thread_count; i++)
		{
			if(pthread_create(&(pool -> threads[i]), NULL, threadpool_thread, (void*)pool) != 0)
			{
				threadpool_destroy(pool, 0);
				return NULL;
			}
			pool -> thread_count++;
			pool -> started++;
		}
		return pool;

	}while(false);

	if(pool != NULL)
	{
		threadpool_free(pool);
	}
	return NULL;
}


int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument, int flags)
{
	int err = 0;
	int next;

	if(pool == NULL || function == NULL)
	{
		return THREADPOOL_INVALID;
	}

	if(pthread_mutex_lock(&(pool -> lock)) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	next = (pool -> tail + 1) % pool -> queue_size;

	do
	{
		// 任务队列是否已满
		if(pool -> count == pool -> queue_size)
		{
			err = THREADPOOL_QUEUE_FULL;
			break;
		}
		
		// 线程池是否已满
		if(pool -> shutdown)
		{
			err = THREADPOOL_SHUTDOWN;
			break;
		}

		// 添加任务到任务队列
		pool -> queue[pool -> tail].function = function;
		pool -> queue[pool -> tail].argument = argument;
		pool -> tail = next;
		pool -> count += 1;

		// 通知线程来取任务
		if(pthread_cond_signal(&(pool -> notify)) != 0)
		{
			err = THREADPOOL_LOCK_FAILURE;
			break;
		}
		
	}while(false);

	if(pthread_mutex_unlock(&pool -> lock) != 0)
	{
		err = THREADPOOL_LOCK_FAILURE;
	}
	return err;
}


int threadpool_destroy(threadpool_t *pool, int flags)
{
	int i, err = 0;

	if(pool == NULL)
	{
		return  THREADPOOL_INVALID;
	}

	if(pthread_mutex_lock(&(pool -> lock)) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	do
	{
		// 线程池是否已停止
		if(pool -> shutdown)
		{
			err = THREADPOOL_SHUTDOWN;
			break;
		}

		pool -> shutdown = (flags & THREADPOOL_GRACEFUL) ? graceful_shutdown : immediate_shutdown;

		// 唤醒所有的线程
		if((pthread_cond_broadcast(&(pool -> notify)) != 0) || 
			(pthread_mutex_unlock(&(pool -> lock)) != 0))
		{
			err = THREADPOOL_LOCK_FAILURE;
			break;
		}

		// 回收所有的线程
		for(i = 0; i < pool -> thread_count; ++i)
		{
			if(pthread_join(pool -> threads[i], NULL) != 0)
			{
				err = THREADPOOL_THREAD_FAILURE;
			}
		}

	}while(false);

	if(!err)
	{
		threadpool_free(pool);
	}
	return err;
}

int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL || pool -> started > 0)
    {
        return -1;
    }

    /* Did we manage to allocate ? */
    if(pool -> threads) 
    {
        free(pool -> threads);
        free(pool -> queue);
 
        /* Because we allocate pool->threads after initializing the
           mutex and condition variable, we're sure they're
           initialized. Let's lock the mutex just in case. */
        pthread_mutex_lock(&(pool -> lock));
        pthread_mutex_destroy(&(pool -> lock));
        pthread_cond_destroy(&(pool -> notify));
    }
    free(pool);    
    return 0;
}

static void *threadpool_thread(void *threadpool)
{
	std::cout << "threadpool_thread" << std::endl;
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    for(;;)
    {
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&(pool -> lock));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        while((pool -> count == 0) && (!pool -> shutdown)) 
        {
            pthread_cond_wait(&(pool -> notify), &(pool -> lock));
        }
		std::cout << "task1" << std::endl;

        if((pool -> shutdown == immediate_shutdown) || ((pool -> shutdown == graceful_shutdown) && (pool -> count == 0)))
        {
			std::cout << "task3" << std::endl;
            break;
        }
		std::cout << "task2" << std::endl;

        /* Grab our task */
        task.function = pool -> queue[pool -> head].function;
        task.argument = pool -> queue[pool -> head].argument;
        pool -> head = (pool -> head + 1) % pool -> queue_size;
        pool -> count -= 1;
		
		std::cout << "task" << std::endl;

        /* Unlock */
        pthread_mutex_unlock(&(pool -> lock));

        /* Get to work */
        (*(task.function))(task.argument);
    }

    --pool -> started;

    pthread_mutex_unlock(&(pool -> lock));
    pthread_exit(NULL);
    return(NULL);
}



void test(void *arg)
{
	std::cout << "arg:" << *((int *)arg) << std::endl;
}

/*
int main(int argc, char *argv[])
{
	int arr[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	threadpool_t *pool = threadpool_create(10, 10, 0);
	if(!pool)
	{
		std::cout << "create threads error." << std::endl;
		//break;
	}

	threadpool_add(pool, test, (void *)arr[0], 0);
	threadpool_add(pool, test, (void *)arr[1], 0);
	threadpool_add(pool, test, (void *)arr[2], 0);
	threadpool_add(pool, test, (void *)arr[3], 0);
	threadpool_add(pool, test, (void *)arr[4], 0);
	threadpool_add(pool, test, (void *)arr[5], 0);
	threadpool_add(pool, test, (void *)arr[6], 0);
	threadpool_add(pool, test, (void *)arr[7], 0);
	threadpool_add(pool, test, (void *)arr[8], 0);
	threadpool_add(pool, test, (void *)arr[9], 0);

	threadpool_destroy(pool, 0);
	threadpool_free(pool);
	return 0;
}
*/
