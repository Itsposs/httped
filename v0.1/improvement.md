# 提升地方

## 具体的细节

* 代码规范 

```c++  
#define UTIL -> #define _UTIL_H_
#endif -> #endif // _UTIL_H_
void setSocket_NonBlocking(int fd) -> void set_socket_nonblocking(int fd)
```

* 尽量使用C++11

  ```c++
  assert() -> satic_assert()
  NULL -> nullptr
  ```

* 裸指针用C++11智能指针

* 解决好出错处理，可以使用异常处理

* 多写注释，这样思路更清晰

* 尽量使用命名空间，防止变量名字污染

* 使用new关键字开辟动态空间，后期使用内存池代替

* 在某些地方写成单例模式

* readn 和 writen 改成写到不能写或读到不能读后加入epoll等待，要记录当前写或读的位置，维护这样一个状态，比较两者的性能差异。

* 信号处理部分可以将 epoll_wait 替换为更安全的 epoll_pwait

* 暂时就这么多，发现还有其它问题再写，水平有限

## 踩坑的地方

  1. 对EPOLLONESHOT的误解，原以为当epoll_wait监听到相应的事件触发后，epoll会把与事件关联的fd从epoll描述符中禁止掉并且彻底删除，实际上并不会删除，man手册上的解释如下：
    EPOLLONESHOT (since Linux 2.6.2)
          
     ```tex
      Sets the one-shot behavior for the associated file descriptor.This means that after an event is pulled out with epoll_wait(2) the associated file descriptor is internally disabled and no other events will be reported by the epoll interface.  The user must call epoll_ctl() with EPOLL_CTL_MOD to rearm the file descriptor with a new event mask.
     ```
     
     

  2. 另外：
      Linux Programmer's Manual 中第六个问题：
         Q6  Will closing a file descriptor cause it to be removed from all
            epoll sets automatically?

         A6  Yes, but be aware of the following point.  A file descriptor is a
            reference to an open file description (see open(2)).  Whenever a
            file descriptor is duplicated via dup(2), dup2(2), fcntl(2)
            F_DUPFD, or fork(2), a new file descriptor referring to the same
            open file description is created.  An open file description con‐
            tinues to exist until all file descriptors referring to it have
            been closed.  A file descriptor is removed from an epoll set only
            after all the file descriptors referring to the underlying open
            file description have been closed (or before if the file descrip‐
            tor is explicitly removed using epoll_ctl(2) EPOLL_CTL_DEL).
            This means that even after a file descriptor that is part of an
            epoll set has been closed, events may be reported for that file
            descriptor if other file descriptors referring to the same under‐
            lying file description remain open.

  当调用close()关闭对应的fd时，会使相应的引用计数减一，只有减到0时，epoll才会真的删掉它，所以，比较安全的做法是：
  先delete掉它，再close它(如果不确定close是否真的关闭了这个文件)。