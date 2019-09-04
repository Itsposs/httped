
#include <unistd.h>
#include <iostream>
#include "ThreadPool.h"

const int QUEUE_SIZE = 65535;
const int THREADPOOL_THREAD_NUM = 4;


void hello(void *args)
{
	std::cout << "hello world!" << std::endl;
}

int main(int argc, char *argv[])
{
	threadpool_t *pool = threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE, 0);
	threadpool_add(pool, hello, NULL, 0);
	threadpool_add(pool, hello, NULL, 0);
	threadpool_add(pool, hello, NULL, 0);
	threadpool_add(pool, hello, NULL, 0);
	sleep(20);
	//threadpool_destroy(pool, 0);
	threadpool_free(pool);
	
	//std::cout << "Hello world!" << std::endl;
	return 0;
}
