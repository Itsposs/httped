#include "epoll.h"
#include <errno.h>
#include <iostream>
#include <sys/epoll.h>
#include "threadpool.h"


struct epoll_event *events;

int epoll_init() {
	int epoll_fd = epoll_create(LISTENQ + 1);
  if(epoll_fd == -1)
		return -1;
	// 动态分配内存可以用全局变量
  events = new epoll_event[MAXEVENTS];
  return epoll_fd;
}

// 注册新描述符
int epoll_add(int epoll_fd, int fd, void *request, __uint32_t events)
{
    struct epoll_event event;
    event.data.ptr = request;
    event.events = events;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
    {
		std::cout << "epoll add error." << std::endl;
        return -1;
    }
    return 0;
}

// 修改描述符状态
int epoll_mod(int epoll_fd, int fd, void *request, __uint32_t events)
{
    struct epoll_event event;
    event.data.ptr = request;
    event.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
    {
        perror("epoll_mod error");
        return -1;
    } 
    return 0;
}

// 从epoll中删除描述符
int epoll_del(int epoll_fd, int fd, void *request, __uint32_t events)
{
    struct epoll_event event;
    event.data.ptr = request;
    event.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) < 0)
    {
        perror("epoll_del error");
        return -1;
    } 
    return 0;
}

// 返回活跃事件数
int my_epoll_wait(int epoll_fd, struct epoll_event* events, int max_events, int timeout)
{
    int count = epoll_wait(epoll_fd, events, max_events, timeout);
    if (count < 0)
    {
		std::cout << "epoll wait error." << std::endl;
		return -1;
    }
    return count;
}
