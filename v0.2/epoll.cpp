
#include "epoll.h"
#include <sys/epoll.h>


struct epoll_event *events;


int epoll_init()
{
	int epoll_fd = ::epoll_create(LISTENQ + 1);
	if(epoll_fd == -1)
		return -1;
	events = new epoll_event[MAXEVENTS];
	return epoll_fd;
}

// test
#include <iostream>

int epoll_add(int epoll_fd, int fd, void * request, __uint32_t events)
{
	struct epoll_event event;
	event.data.ptr = request;
	event.events = events;

	if(::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
	{
		std::cout << "epoll_fd error!" << std::endl;
		return -1;
	}
	std::cout << "epoll_add ok!" << std::endl;
	return 0;
}

int epoll_mod(int epoll_fd, int fd, void * request, __uint32_t events)
{
	struct epoll_event event;
	event.data.ptr = request;
	event.events = events;

	if(::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
		return -1;
	return 0;
}

int epoll_del(int epoll_fd, int fd, void * request, __uint32_t events)
{
	struct epoll_event event;
	event.data.ptr = request;
	event.events = events;

	if(::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) < 0)
		return -1;
	return 0;
}

int my_epoll_wait(int epoll_fd, struct epoll_event *events, int max_events, int timeout)
{
	int ret_count = ::epoll_wait(epoll_fd, events, max_events, timeout);
	if(ret_count < 0)
		return -1;
	return ret_count;
}
