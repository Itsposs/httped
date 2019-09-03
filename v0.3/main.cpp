#include "Util.h"
#include "Epoll.h"


#include <string.h> // memset()

#include <sys/socket.h> // socket()

#include <sys/unistd.h>  // close()

#include <strings.h> // bzero()

#include <arpa/inet.h>  // htonl() htons()

#include <sys/epoll.h>

#include <queue>  // priorty_queue
#include <string>  // string
#include "ThreadPool.h"
#include "RequestData.h"

// test
#include <iostream>


// port
const int PORT = 8888; 
const int LISTENQ = 1024;

const int MAXEVENTS = 5000;


// threadpool
const int QUEUE_SIZE = 65535;
const int THREADPOOL_THREAD_NUM = 4;

// time
const int TIMER_TIME_OUT = 500;

const std::string PATH = "/";
extern std::priority_queue<std::shared_ptr<mytimer>, std::deque<std::shared_ptr<mytimer>>, timerCmp> myTimerQueue;

int socket_bind_listen(int port)
{
	if(port < 1024 || port > 65535)
		return -1;
	
	int listen_fd = 0;
	if((listen_fd = ::socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;
	
	// Address already in use
	int optval = 1;
	if(::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
		return -1;
	
	struct sockaddr_in server_addr;
	::bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
	server_addr.sin_port = htons((unsigned short)port);

	if(::bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
		return -1;
	
	// 开始监听,最大等待队列长为SOMAXCONN
	if(::listen(listen_fd, LISTENQ) == -1)
		return -1;
	// 无效监听描述符
	if(listen_fd == -1)
	{
		::close(listen_fd);
		return -1;
	}
	return listen_fd;
}


// 处理逻辑:
// 优先级队列不支持随机访问
// 即使支持,随机删除某个节点后破坏了堆的结构,需要更新堆结构.
// 所以对于被置为deleted的节点,会延迟到它超时或它前面的节点都被删除了,它才会被删除.
// 一个节点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除
// 好处:
// 不需要遍历队列,省时.
// 给超时时间一个容忍的时间,就是设定的超时时间是删除的下限(并不是一道超时时间就立即删除),
// 如果请求在超时后下一次请求中又一次出现了,就不需要重新申请RequestData节点了,这样就可以
// 重新申请RequestData节点了,这样就可以重复利用前面的RequestData,减少delete和new的时间.

void handle_expired_event()
{
	MutexLockGuard lock;
	while(!myTimerQueue.empty())
	{
		std::shared_ptr<mytimer> ptimer_now = myTimerQueue.top();
		if(ptimer_now -> isDeleted())
		{
			myTimerQueue.pop();
		}
		else if(ptimer_now -> isvalid() == false)
		{
			myTimerQueue.top();
		}
		else
			break;
	}
}

int main(int argc, char *argv[])
{
	handle_for_sigpipe();
	if(Epoll::epoll_init(MAXEVENTS, LISTENQ) < 0)
	{
		perror("epoll init failed");
		return 1;
	}

	if(ThreadPool::threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE) < 0)
	{
		perror("Threadpool create failed");
		return 1;
	}
	int listen_fd = socket_bind_listen(PORT);
	if(listen_fd < 0)
	{
		perror("socket bind failed");
		return 1;
	}
	if(setSocketNonBlocking(listen_fd) < 0)
	{
		perror("set socket non block failed");
		return 1;
	}

	std::shared_ptr<RequestData> request(new RequestData());
	request -> setFd(listen_fd);
	
	//__uint32_t events = EPOLLIN | EPOLLET;
	if(Epoll::epoll_add(listen_fd, request, EPOLLIN | EPOLLET) < 0)
	{
		perror("epoll add error");
		return 1;
	}

	while(true)
	{
		Epoll::my_epoll_wait(listen_fd, MAXEVENTS, -1);
		handle_expired_event();
	}
	
	return 0;
}

