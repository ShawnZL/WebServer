# 文件说明

理解epoll原理，因为epoll是在Linux上的实现方式，在Mac os上使用了全新的方式kqueue，所以尝试多写一个kqueue的控制方式

## 运行方法

```shell
gcc server.c -o serv
./serv 9190
```

# epoll

Select复用服务器， 调用select两数后，并不是把发生变化的文件描述符单独集中到一起，而是通过观察作为监视对象的fd_ set 变量的 变化 ， 找出发生变化的文件描述符，因此无法避免针对所有监视对象的循环语句。同时还牵扯到了将文件描述符集合由用户态放进内核态进行监听，然后不断轮询，再将文件描述符集合从内核复制出进行处理。

```c
typedef union epoll_data
{
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

struct epoll_event
{
  uint32_t events;  /* Epoll events */
  epoll_data_t data;    /* User data variable */
};
```

声明足够大的epoll_event结构体数组后，传递给epoll_wait函数，发生变化的文件描述符信息将被填入该数组。

```c
int epoll_create(int size); // size epoll 大小
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
// epfd 用于注册监视对象的epoll例程的文件描述符
// op 用于指定监视对象的添加、刪除或更改等操作。
// fd 需要注册的监视对象文件描述符。
// event 监视对象的事件类型。
int epoll_wait(int epfd, struct epoll_event* event, int maxevent, int timeout);
// epfd 用于注册监视对象的epoll例程的文件描述符
// event 监视对象的事件类型。
// maxevent 可以保存的最大事件数目
// timeout 以1/1000秒为单位的等待时间，传递-1时，一直等待直到发生事件。
```

## 水平触发（条件Level）与垂直触发（边缘Edge）

条件触发方式中，只要输入缓冲有数据就会 一直通知该事件。

边缘触发中输人缓冲收到数据时仅注册1次该事件。即使输人缓冲中还留有数据，也不会再进行注册。

## 垂直触发

通过errno变量验证错误原因

为了完成非阻塞IO，更改套接字特性

```c
int fcntl(int filedes, int cmd, ...);
// filedes 属性更改目标的文件描述符
// cmd 表示函数调用的目的

int flag = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flag|O_NONBLOCK);
```

**可以分离接收数据和处理数据的时间点!**