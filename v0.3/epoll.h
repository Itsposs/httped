

#ifndef _EPOLL_H_
#define _EPOLL_H_

#include <vector>
#include <memory>
#include <sys/epoll.h>
#include <unordered_map>
#include "requestData.h"


class Epoll
{
	public:
		static int epoll_init(int maxevents, int listen_num);
		static int epoll_add(int fd, std::shared_ptr<requestData> request, __uint32_t events);
		static int epoll_mod(int fd, std::shared_ptr<requestData> request, __uint32_t events);
		static int epoll_del(int fd, __uint32_t events);
		static void  my_epoll_wait(int listen_fd, int max_events, int timeout);
		static void acceptConnection(int listen_fd, int epoll_fd, const std::string path);
		static std::vector<std::shared_ptr<requestData>> getEventsRequest(int listen_fd, int events_num, const std::string path);
	private:
		static int epoll_fd;
		static epoll_event  *events;
		static const std::string PATH;
		static std::unordered_map<int, std::shared_ptr<requestData>> fd2req;
};



#endif // _EPOLL_H_
