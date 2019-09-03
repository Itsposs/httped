#include "util.h"
#include "epoll.h"

#include <assert.h>

#include <string.h> // memset()

#include <sys/socket.h> // socket()

#include <sys/unistd.h>  // close()

#include <strings.h> // bzero()

#include <arpa/inet.h>  // htonl() htons()

#include <sys/epoll.h>

#include <queue>  // priorty_queue
#include <string>  // string
#include "threadpool.h"
#include "requestdata.h"

// test
#include <iostream>


// port
const int PORT = 8000 ; 

// threadpool
const int QUEUE_SIZE = 65535;
const int THREADPOOL_THREAD_NUM = 4;

// time
const int TIMER_TIME_OUT = 500;

const std::string PATH = "/";
extern struct epoll_event *events;
extern std::priority_queue<mytimer *, std::deque<mytimer *>, timerCmp> myTimerQueue;

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
	server_addr.sin_port = htons(port);

	if(::bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
		return -1;
	
	// 开始监听,最大等待队列长度为SOMAXCONN
	if(::listen(listen_fd, SOMAXCONN) == -1)
		return -1;
	
	// 无效监听描述符
	if(listen_fd == -1)
	{
		::close(listen_fd);
		return -1;
	}
	return listen_fd;
}

void myHandler(void *args)
{
	requestData *req_data = (requestData *)args;
	req_data -> handleRequest();
}

void acceptConnection(int listen_fd, int epoll_fd, const std::string &path)
{
	std::cout << "come in acceptConnection" << std::endl;
	struct sockaddr_in client_addr;
	::memset(&client_addr, 0, sizeof(struct sockaddr_in));
	socklen_t client_addr_len = 0;
	int accept_fd = 0;
	std::cout << "epoll_fd:" << epoll_fd <<  std::endl;	
	while((accept_fd = ::accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len)) > 0)
	{
		std::cout << "accept" << std::endl;
		// nonblock
		int ret = ::setSocketNonBlocking(accept_fd);
		if(ret < 0)
			return;
		
		requestData *req_info = new requestData(path, accept_fd, epoll_fd);

		__uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
		::epoll_add(epoll_fd, accept_fd, static_cast<void *>(req_info), _epo_event);
		
		std::cout << " add time" << std::endl;

		// add time
		mytimer *mtimer = new mytimer(req_info, TIMER_TIME_OUT);
		req_info -> addTimer(mtimer);
		MutexLockGuard();
		myTimerQueue.push(mtimer);

	}
}

void handle_events(int epoll_fd, int listen_fd, struct epoll_event *events, int events_num,
	const std::string &path, threadpool_t *tp)
{
	for(int i = 0; i < events_num; i++)
	{
		// 获取有事件产生的描述符
		requestData *request = (requestData *)(events[i].data.ptr);
		int fd = request -> getFd();

		// 监听描述符
		if(fd == listen_fd)
		{
			std::cout << "this is a listen_fd" << std::endl;
			::acceptConnection(listen_fd, epoll_fd, path);
		}
		else
		{
			// error
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) 
				|| (!(events[i].events & EPOLLIN)))
			{
				delete request;
				continue;
			}

			// 将timer和request分离
			// 将请求任务加入到线程池中
			request -> seperateTimer();
			int rc = ::threadpool_add(tp, myHandler, events[i].data.ptr, 0);
			std::cout << "rc:" << rc << std::endl;
		}
	}
}


// 处理逻辑:
// 优先级队列不支持随机访问
// 即使支持,随机删除某个节点后破坏了堆的结构,需要更新堆结构.
// 所以对于被置为deleted的节点,会延迟到它超时或它前面的节点都被删除了,它才会被删除.
// 一个节点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除
// 好处:
// 不需要遍历队列,省时.
// 给超时时间一个容忍的时间,就是设定的超时时间是删除的下限(并不是一道超时时间就立即删除),
// 如果请求在超时后下一次请求中又一次出现了,就不需要重新申请requestData节点了,这样就可以
// 重新申请requestData节点了,这样就可以重复利用前面的requestData,减少delete和new的时间.

void handle_expired_event()
{
	MutexLockGuard();
	while(!myTimerQueue.empty())
	{
		mytimer *ptimer_now = myTimerQueue.top();
		if(ptimer_now -> isDeleted())
		{
			myTimerQueue.pop();
			delete ptimer_now;
		}
		else if(ptimer_now -> isvalid() == false)
		{
			myTimerQueue.top();
			delete ptimer_now;
		}
		else
			break;
	}
}

int main(int argc, char *argv[])
{
	handle_for_sigpipe();
	int epoll_fd = epoll_init();
	assert(epoll_fd > 0);

	threadpool_t *threadpool = threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE, 0);
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

	__uint32_t event = EPOLLIN | EPOLLET;
	requestData *req = new requestData();
	req -> setFd(listen_fd);
	epoll_add(epoll_fd, listen_fd, static_cast<void*>(req), event);

	while(true)
	{
		int events_num = ::my_epoll_wait(epoll_fd, events, MAXEVENTS, -1);
		std::cout << "events_num:" << events_num << std::endl;
		if(events_num == 0)
			continue;
		handle_events(epoll_fd, listen_fd, events, events_num, PATH, threadpool);
		handle_expired_event();
	}
	
	return 0;
}

