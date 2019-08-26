

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <vector>
#include <memory>
#include <pthread.h>
#include <functional>


typedef enum 
{
	immediate_shutdown = 1,
	graceful_shutdown = 2
}threadpool_shutdown_t;


struct ThreadPoolTask
{
	std::function<void(std::shared_ptr<void>)> fun;
	std::shared_ptr<void> args;
};


void myHandler(std::shared_ptr<void> req);
class ThreadPool
{
	public:
		static int threadpool_create(int _thread_count, int _queue_size);
		static int threadpool_add(std::shared_ptr<void> args, std::function<void(std::shared_ptr<void>)> fun = myHandler);
		static int threadpool_destroy();
		static int threadpool_free();
		static void * threadpool_thread(void *args);


	private:
		static pthread_mutex_t lock;
		static pthread_cond_t notify;
		static std::vector<pthread_t> threads;
		static std::vector<ThreadPoolTask> queue;
		static int thread_count;
		static int queue_size;
		// tail指向尾节点的下一节点
		static int head;
		static int tail;
		static int count;
		static int shutdown;
		static int started;
};



#endif
