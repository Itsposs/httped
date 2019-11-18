#include <queue>
#include "util.h"
#include "epoll.h"
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "threadpool.h"


const int THREADPOOL_THREAD_NUM = 4;
const int QUEUE_SIZE = 65535;

const int PORT = 8000;
const int ASK_STATIC_FILE = 1;
const int ASK_IMAGE_STITCH = 2;

const std::string PATH = "/";

const int TIMER_TIME_OUT = 500;


// 前面已声明
extern pthread_mutex_t qlock;
extern struct epoll_event* events;
extern std::priority_queue<mytimer*, std::deque<mytimer*>, timerCmp> myTimerQueue;

int socket_bind_listen(int port) 
{
	// 检查port值，取正确区间范围
  if (port < 1024 || port > 65535)
		return -1;

  // 创建socket(IPv4 + TCP)，返回监听描述符
  int listen_fd = 0;
  if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
      return -1;

  // 消除bind时"Address already in use"错误
  int optval = 1;
  if(setsockopt(listen_fd, SOL_SOCKET,  SO_REUSEADDR, &optval, sizeof(optval)) == -1)
		return -1;

  // 设置服务器IP和Port，和监听描述副绑定
  struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons((unsigned short)port);
  if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
		return -1;

    // 开始监听，最大等待队列长为LISTENQ
    if(listen(listen_fd, LISTENQ) == -1)
        return -1;

    // 无效监听描述符
    if(listen_fd == -1)
    {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

void myHandler(void *args) 
{
	std::cout << "myHandler"  << std::endl;
	requestData *req_data = (requestData*)args;
	req_data -> handleRequest();
}

void acceptConnection(int listen_fd, int epoll_fd, const std::string &path)
{
	std::cout << "acceptConnection" << std::endl;
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	socklen_t client_addr_len = 0;
	int accept_fd = 0;
	accept_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
	std::cout << "accept fd: " << accept_fd << std::endl;
		
	// TCP的保活机制默认是关闭的
	//int optval = 0;
	//socklen_t len_optval = 4;
	//setsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
	//cout << "optval ==" << optval << endl;
       
	std::cout << "client port:" << client_addr.sin_port << std::endl;
	std::cout << "client addr:" << client_addr.sin_addr.s_addr << std::endl;

	// 设为非阻塞模式
	int ret = set_socket_nonblocking(accept_fd);
	if (ret < 0) {
		perror("Set non block failed!");
		return;
	}

	requestData *req_info = new requestData(epoll_fd, accept_fd, path);

	// 文件描述符可以读，边缘触发(Edge Triggered)模式，保证一个socket连接在任一时刻只被一个线程处理
	__uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_add(epoll_fd, accept_fd, static_cast<void*>(req_info), _epo_event);
        
	// 新增时间信息
	mytimer *mtimer = new mytimer(req_info, TIMER_TIME_OUT);
	req_info -> addTimer(mtimer);
	pthread_mutex_lock(&qlock);
	myTimerQueue.push(mtimer);
	pthread_mutex_unlock(&qlock);
}


// 分发处理函数
void handle_events(int epoll_fd, int listen_fd, struct epoll_event* events, 
		int events_num, const std::string &path, threadpool_t* tp) 
{
	std::cout << "events_num :" << events_num << std::endl;		
	for (int i = 0; i < events_num; i++) 
	{
		// 获取有事件产生的描述符
		requestData* request = (requestData*)(events[i].data.ptr);
		int fd = request -> getFd();
		// 有事件发生的描述符为监听描述符
		if (fd == listen_fd) 
		{
			std::cout << "This is a listen fd" << std::endl;
			// 接受连接
			acceptConnection(listen_fd, epoll_fd, path);
		} 
		else 
		{
			std::cout << "This isn't a listen fd" << std::endl;
			// 排除错误事件
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
					|| (!(events[i].events & EPOLLIN))) 
			{
				delete request;
				continue;
			}
			// 将请求任务加入到线程池中
			// 加入线程池之前将Timer和request分离
			request -> seperateTimer();
			threadpool_add(tp, myHandler, events[i].data.ptr);
		}
	}
}

/* 处理逻辑是这样的~
因为:
	(1) 优先队列不支持随机访问
	(2) 即使支持，随机删除某节点后破坏了堆的结构，需要重新更新堆结构。
所以对于被置为deleted的时间节点，会延迟到它(1)超时 或 (2)它前面的节点都被删除时，它才会被删除。

一个点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除。
这样做有两个好处：
(1) 第一个好处是不需要遍历优先队列，省时。
(2) 第二个好处是给超时时间一个容忍的时间，就是设定的超时时间是删除的下限(并不是一到超时时间就立即删除)，
    如果监听的请求在超时后的下一次请求中又一次出现了,就不用再重新申请requestData节点了，这样可以继续重
	复利用前面的requestData，减少了一次delete和一次new的时间。
*/

void handle_expired_event() 
{
	std::cout << "handle_expired_event" << std::endl;
	pthread_mutex_lock(&qlock);
	while (!myTimerQueue.empty()) 
	{
		std::cout << "TimerQueue:" << myTimerQueue.size() << std::endl;
		mytimer *ptimer_now = myTimerQueue.top();
		if (ptimer_now -> isDeleted()) 
		{
			std::cout << "isDeleted" << std::endl;
			myTimerQueue.pop();
			delete ptimer_now;
		} 
		else if (ptimer_now -> isvalid() == false) 
		{
			std::cout << "isvalid" << std::endl;
			myTimerQueue.pop();
			delete ptimer_now;
		} 
		else
			break;
	}
	pthread_mutex_unlock(&qlock);
}


int main(int argc, char *argv[]) 
{
  handle_for_sigpipe();
  int epoll_fd = epoll_init();
	
  // 创建线程池
  threadpool_t *threadpool = threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE);

	int listen_fd = socket_bind_listen(PORT);
	
	// 出错处理
	if (listen_fd < 0) {
		perror("socket bind failed");
		return 1;
	}
	
	if (set_socket_nonblocking(listen_fd) < 0) {
		perror("set socket non block failed");
		return 1;
	}
	// 考虑用智能指针
	requestData *req = new requestData();
	req -> setFd(listen_fd);

	// epoll ET模式
	__uint32_t event = EPOLLIN | EPOLLET;
	epoll_add(epoll_fd, listen_fd, static_cast<void*>(req), event);
    
	while (true) 
	{
		std::cout << "main::while" << std::endl;
		int events_num = my_epoll_wait(epoll_fd, events, MAXEVENTS, -1);
		//printf("%zu\n", myTimerQueue.size());        
		if (events_num == 0)
			continue;
		else
			std::cout << "one connection has come!" << std::endl;

		// 遍历events数组，根据监听种类及描述符类型分发操作
		handle_events(epoll_fd, listen_fd, events, events_num, PATH, threadpool);
		handle_expired_event();
	}
	return 0;
}
