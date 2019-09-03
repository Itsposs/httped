#include "util.h"
#include "epoll.h"


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
const int PORT = 8888; 

const int LISTENQ = 1024;
const int MAXEVENTS = 5000;


// threadpool
const int QUEUE_SIZE = 65535;
const int THREADPOOL_THREAD_NUM = 4;

// time
const int TIMER_TIME_OUT = 500;

const std::string PATH = "/";

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
	// error
	if(listen_fd == -1)
	{
		::close(listen_fd);
		return -1;
	}
	return listen_fd;
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

	if(Epoll::epoll_add(listen_fd, request, EPOLLIN | EPOLLET) < 0)
	{
		perror("epoll add error");
		return 1;
	}

	while(true)
	{
		Epoll::my_epoll_wait(listen_fd, MAXEVENTS, -1);
		//handle_expired_event();
	}
	
	return 0;
}

