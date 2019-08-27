
#ifndef _UTIL_H_
#define _UTIL_H_

#include <string>
#include <cstdlib>


ssize_t readn(int fd, std::string &inBuffer);
ssize_t writen(int fd, std::string &sbuff);
ssize_t readn(int fd, void *buff, size_t n);
ssize_t writen(int fd, void *buff, size_t n);
void handle_for_sigpipe();
int setSocketNonBlocking(int fd);


#endif // _UTIL_H_
