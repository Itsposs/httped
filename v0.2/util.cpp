
#include "util.h"
#include <signal.h>   // sigemptyset() sigaction()


void handle_for_sigpipe(void)
{
	struct sigaction sa;
	// 清除信号
	sigemptyset(&sa.sa_mask);
	// 有信号中断时,默认忽略
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	// 当类型为SOCK_STREAM的套接字已不再连接时,
	// 进程写该套接字也产生此信号
	if(0 == sigaction(SIGPIPE, &sa, NULL)) // ???
		return;
}

// 设置非阻塞IO
int set_socket_nonblock(int fd)
{
	int flag = 0;
	if(-1 == (flag = fcntl(fd, F_GETFL, 0)))
}


ssize_t readn(int fd, void *buff, size_t n)
{
	size_t nleft = 0;
	ssize_t nread = 0;
	char *ptr = nullptr;
	// 强制类型转换,尽量不要使用(char *)buff;
	ptr = static_cast<char *>buff;
	nleft = n;

	while(nleft > 0)
	{
		if(nread = read(fd, ptr, nleft) < 0)
		{
			// 信号中断函数调用,errno设为EINTR
			if(EINTR == errno)
				nread = 0;
			else if(EAGIN == errno)
				return -1;
		}
		else if(0 == nread)
			break;        // EOF
		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft);    // return >= 0
}
