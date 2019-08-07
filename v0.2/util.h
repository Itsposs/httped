
#define _UTIL_H_
#define _UTIL_H_

#include <cstdlib>

ssize_t  readn(ing fd, void *buff, size_t n);
ssize_t writen(int fd, void *buff, size_t n);
void handle_for_sigpipe(void);
int set_socket_nonblock(int fd);


#endif // _UTIL_H_
