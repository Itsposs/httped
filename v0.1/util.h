#ifndef _UTIL_H_
#define _UTIL_H_

#include <cstdlib>

void handle_for_sigpipe();
int set_socket_nonblocking(int fd);
ssize_t readn(int fd, void *buff, size_t n);
ssize_t writen(int fd, void *buff, size_t n);



#endif  // _UTIL_H_
