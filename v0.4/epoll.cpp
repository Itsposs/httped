#include "util.h"
#include <queue>
#include "epoll.h"
#include <string.h>
#include <unistd.h>   // close()
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "threadpool.h"
#include "requestdata.h"

const int TIMER_TIME_OUT = 500;

int Epoll::epoll_fd = 0;
epoll_event * Epoll::events;
const std::string Epoll::PATH = "/";
Epoll::SP_ReqData Epoll::fd2req[MAXFDS];
TimerManager Epoll::timer_manager;

int Epoll::epoll_init(int maxevents, int listen_num)
{
	epoll_fd = ::epoll_create(listen_num + 1);
	if(epoll_fd == -1)
		return -1;
	events = new epoll_event[maxevents];
	return 0;
}


int Epoll::epoll_add(int fd, std::shared_ptr<RequestData> request, __uint32_t events)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = events;

	if(::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
		return -1;
	fd2req[fd] = request;
	return 0;
}

int Epoll::epoll_mod(int fd, std::shared_ptr<RequestData> request, __uint32_t events)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = events;

	if(::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
	{
		fd2req[fd].reset();
		return -1;
	}
	return 0;
}

int Epoll::epoll_del(int fd, __uint32_t events)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = events;

	if(::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) < 0)
		return -1;
	fd2req[fd].reset();
	return 0;
}

void Epoll::my_epoll_wait(int listen_fd, int max_events, int timeout)
{
	int event_count = ::epoll_wait(epoll_fd, events, max_events, timeout);
	if(event_count < 0)
		perror("epoll wait error");
	std::vector<std::shared_ptr<RequestData>> req_data = getEventsRequest(listen_fd, event_count, PATH);
	if(req_data.size())
	{
		for(auto &req : req_data)
		{
			if(ThreadPool::threadpool_add(req) < 0)
			{
				// 线程池满了或者关闭了等原因,抛弃本次监听到的请求
				break;
			}
		}
	}
}


void Epoll::acceptConnection(int listen_fd, int epoll_fd, const std::string path)
{
	struct sockaddr_in client_ddr;
	::memset(&client_ddr, 0, sizeof(struct sockaddr_in));
	socklen_t client_addr_len = sizeof(client_ddr);
	int accept_fd = 0;
	while((accept_fd = ::accept(listen_fd, (struct sockaddr *)&client_ddr, &client_addr_len)) > 0)
	{
		/*
		// Tcp的保活机制默认是关闭的
		int optval = 0;
		socklen_t len_optval = 4;
		::getsockopt(accept_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, &len_optval);
		std::cout << "optval == " << optval << std::endl;
		*/

		// 限制服务器最大并发连接数
		if(accept_fd >= MAXFDS)
		{
			close(accept_fd);
			continue;
		}

		int ret = setSocketNonBlocking(accept_fd);
		if(ret < 0)
		{
			perror("set on block error!");
			return;
		}

		std::shared_ptr<RequestData> req_info(new RequestData(epoll_fd, accept_fd, path));

		// 文件描述符可读,边缘触发(Edge Triggered)模式,保证任何时刻一个socket连接只被一个线程处理
		__uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
		Epoll::epoll_add(accept_fd, req_info, _epo_event);

		// 时间信息
		timer_manager.addTimer(req_info, TIMER_TIME_OUT);
	}
}

// 分发处理函数
std::vector<std::shared_ptr<RequestData>> Epoll::getEventsRequest(int listen_fd, int events_num, const std::string path)
{
	std::vector<std::shared_ptr<RequestData>> req_data;
	for(int i = 0; i < events_num; ++i)
	{
		// 获取有事件产生的描述符
		int fd = events[i].data.fd;

		// 有事件发生的描述符为监听描述符
		if(fd == listen_fd)
		{
			// std::cout << "This is listen_fd" << std::endl;
			acceptConnection(listen_fd, epoll_fd, path);
		}
		else if(fd < 3)
			break;
		else
		{
			// 排除错误事件
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
				|| (!(events[i].events & EPOLLIN)))
			{
				if(fd2req[fd])
					fd2req[fd] -> seperateTimer();
				fd2req[fd].reset();
				continue;
			}

			// 将请求任务加入到线程中
			// 加入线程池之前将timer和request分离
			SP_ReqData cur_req = fd2req[fd];

			if(cur_req)
			{
				if((events[i].events & EPOLLIN) || (events[i].events & EPOLLPRI))
					cur_req -> enableRead();
				else
					cur_req -> enableWrite();
				cur_req -> seperateTimer();
				req_data.push_back(cur_req);
				fd2req[fd].reset();
			}
			else
				std::cout << "SP cur_req is invalid" << std::endl;
		}
	}
	return req_data;
}


void Epoll::add_timer(std::shared_ptr<RequestData> request_data, int timeout)
{
	timer_manager.addTimer(request_data, timeout);
}
