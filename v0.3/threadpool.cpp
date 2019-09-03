
#include <cstdlib>
#include "threadpool.h"
#include "requestdata.h"

int ThreadPool::head = 0;
int ThreadPool::tail = 0;
int ThreadPool::count = 0;
int ThreadPool::started = 0;
int ThreadPool::shutdown = 0;
int ThreadPool::queue_size = 0;
int ThreadPool::thread_count = 0;
std::vector<pthread_t> ThreadPool::threads;
std::vector<ThreadPoolTask> ThreadPool::queue;
pthread_mutex_t ThreadPool::lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  ThreadPool::notify = PTHREAD_COND_INITIALIZER;



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



int ThreadPool::threadpool_create(int _thread_count, int _queue_size)
{
	bool err = false;
	// do 
	// 可以将语句当作一个独立的域
	// 对于多语句可以正常的运行
	// 可以有效的消除goto语句,达到跳转语句的功能
	do
	{
		if(thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
		{
			_thread_count = 4;
			_queue_size = 1024;
		}


		// 初始化
		thread_count = 0;
		queue_size = queue_size;
		head = tail = count = 0;
		shutdown = started = 0;

		// 为线程池和任务队列分配内存空间
		threads.resize(_thread_count);
		queue.resize(_queue_size);


		// 开始任务线程
		for(int i = 0; i < _thread_count; i++)
		{
			if(pthread_create(&threads[i], NULL, threadpool_thread, (void*)(0)) != 0)
			{
				//threadpool_destroy(pool, 0);
				return -1;
			}
			++thread_count;
			++started;
		}

	}while(false);

	if(err)
	{
		//threadpool_free(pool);
		return -1;
	}
	return 0;
}

void myHandler(std::shared_ptr<void> req)
{
	std::shared_ptr<requestData> request = std::static_pointer_cast<requestData> (req);
	request -> handleRequest();
}


int ThreadPool::threadpool_add(std::shared_ptr<void> args, std::function<void(std::shared_ptr<void>)> fun)
{
	int err = 0, next = 0;

	if(pthread_mutex_lock(&lock) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}


	do
	{
		next = (tail + 1) % queue_size;
		// 任务队列是否已满
		if(count == queue_size)
		{
			err = THREADPOOL_QUEUE_FULL;
			break;
		}
		
		// 线程池是否已满
		if(shutdown)
		{
			err = THREADPOOL_SHUTDOWN;
			break;
		}

		// 添加任务到任务队列
		queue[tail].fun = fun;
		queue[tail].args = args;
		tail = next;
		++count;

		// 通知线程来取任务
		if(pthread_cond_signal(&notify) != 0)
		{
			err = THREADPOOL_LOCK_FAILURE;
			break;
		}
		
	}while(false);

	if(pthread_mutex_unlock(&lock) != 0)
	{
		err = THREADPOOL_LOCK_FAILURE;
	}
	return err;
}

/*
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

    if(pool -> threads) 
    {
        free(pool -> threads);
        free(pool -> queue);
 
        //Because we allocate pool->threads after initializing the
        //mutex and condition variable, we're sure they're
        //initialized. Let's lock the mutex just in case.
        pthread_mutex_lock(&(pool -> lock));
        pthread_mutex_destroy(&(pool -> lock));
        pthread_cond_destroy(&(pool -> notify));
    }
    free(pool);    
    return 0;
}
*/

void * ThreadPool::threadpool_thread(void *args)
{

    while(true)
    {
		ThreadPoolTask task;
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&lock);

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        while((count == 0) && (!shutdown)) 
        {
            pthread_cond_wait(&notify, &lock);
        }
		std::cout << "task1" << std::endl;

        if((shutdown == immediate_shutdown) || 
			((shutdown == graceful_shutdown) && (count == 0)))
        {
            break;
        }

        /* Grab our task */
        task.fun = queue[head].fun;
        task.args = queue[head].args;
		queue[head].fun = NULL;
		queue[head].args.reset();
        head = (head + 1) % queue_size;
        --count;
		

        /* Unlock */
        pthread_mutex_unlock(&lock);

        /* Get to work */
        (task.fun)(task.args);
    }

    --started;

    pthread_mutex_unlock(&lock);
    pthread_exit(NULL);
    return(NULL);
}



