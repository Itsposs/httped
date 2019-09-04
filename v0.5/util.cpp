#include "util.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <string.h> // memset()

#include <iostream> // test

const int MAX_BUFF = 4096;

// 适度修改
ssize_t readn(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nread = 0;
    ssize_t readSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0;
            else if (errno == EAGAIN)
            {
                return readSum;
            }
            else
            {
                return -1;
            }  
        }
        else if (nread == 0)
            break;
        readSum += nread;
        nleft -= nread;
        ptr += nread;
    }
    return readSum;
}

ssize_t readn(int fd, std::string &inBuffer)
{
	ssize_t nread = 0;
	ssize_t readSum = 0;

	while(true)
	{
		char buff[MAX_BUFF];
		if((nread = ::read(fd,buff, MAX_BUFF)) < 0)
		{
			if(errno == EINTR)
				continue;
			else if(errno == EAGAIN)
				return readSum;
			else
				return -1;
		}
		else if(nread == 0)
			break;
		readSum += nread;
		inBuffer += std::string(buff, buff + nread);
	}
	return readSum;
}


ssize_t writen(int fd, std::string &sbuff)
{
	size_t nleft = sbuff.size();
	ssize_t nwritten = 0;
	ssize_t writeSum = 0;
	const char *ptr = sbuff.c_str();
	while(nleft > 0)
	{
		if((nwritten = ::write(fd, ptr, nleft)) <= 0)
		{
			if(nwritten < 0)
			{
				if(errno == EINTR)
				{
					nwritten = 0;
					continue;
				}
				else if(errno == EAGAIN)
					break;
				else
					return -1;
			}
		}
		writeSum += nwritten;
		nleft -= nwritten;
		ptr += nwritten;
	}
	if(writeSum == sbuff.size())
		sbuff.clear();
	else
		sbuff = sbuff.substr(writeSum);
	return writeSum;
}


ssize_t writen(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    nwritten = 0;
                    continue;
                }
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return writeSum;
}

void handle_for_sigpipe()
{
	std::cout << "handle_for_sigpipe" << std::endl;
    struct sigaction sa;
	// 清除信号sigemptyset()
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
	// 当类型为SOCK_STREAM的套接字已不再连接时,
	// 进程该套接字也产生此信号
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}

// 出错处理
int setSocketNonBlocking(int fd)
{
	std::cout << "setSocketNonBlocking" << std::endl;
    int flag = fcntl(fd, F_GETFL, 0);
    if(flag == -1)
        return -1;

    flag |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flag) == -1)
        return -1;
    return 0;
}
