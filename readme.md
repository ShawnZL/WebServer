# 文件说明

理解epoll原理，因为epoll是在Linux上的实现方式，在Mac os上使用了全新的方式kqueue，所以尝试多写一个kqueue的控制方式

[文档📄](https://github.com/ShawnZL/Socket_Learning/blob/master/TCP_Reading.md)

## 运行方法

```shell
gcc server.c -o serv
./serv 9190
./cli 127.0.0.1 9190
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

## 条件触发和边缘触发孰优孰劣

#### 条件触发

只要输入缓冲有数据就会一直触发事件，直到缓冲区没有数据。

```c
#include <unistd.h>
#include <stdio.h>
#include <sys/epoll.h>
int main() {
    int epfd, nfds;
    char buf[256];
    struct epoll_event event, events[5];
    epfd=epoll_create(1);
    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN; //LT默认模式
    epoll_ctl(epfd,EPOLL_CTL_ADD,STDIN_FILENO, &event);
    while (1) {
        nfds = epoll_wait(epfd, events, 5, -1);
        int i;
        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == STDIN_FILENO);
            //read(STDIN_FILENO, buf, sizeof(buf));
            printf("hello world\n");
        }
    }
    return 0;
}
```



使用键盘输入字符串的时候，字符串会输入到STDIN_FILENO文件描述符中亦或是在其缓冲中，但是此时没有read函数将其读出，所以就一直存储在缓冲中，因此会一直有条件触发。

#### 边缘触发

```c 
#include <unistd.h>
#include <stdio.h>
#include <sys/epoll.h>
 
int main(int argc, char *argv[])
{
    int epfd, nfds;
    char buf[256];
    struct epoll_event event, events[5];
    epfd = epoll_create(1);
    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN | EPOLLET;  // 加入EPOLLET即可变为边缘模式
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event);

    while (1) {
        nfds = epoll_wait(epfd, events, 5, -1); // 
        int i;
        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == STDIN_FILENO) {
                //read(STDIN_FILENO, buf, sizeof(buf));
                printf("hello world\n");
            }
        }
    }
    return 0;
}
```

# kqueue

原理同epoll，但是因为epoll为Linux专属，Mac上使用的kqueue，所以将epoll里面的事件注册机制更换为kqueue进行处理。简单解释，Kqueue是unix系统上高效的IO多路复用技术（常见的io复用有select、poll、epoll、kqueue等等，其中epoll为Linux系统独有，kqueue则在众多unix系统中存在）。

kqueue与epoll非常相似，在注册一批文件描述符到 kqueue 以后，当其中的描述符状态发生变化时，kqueue将一次性通知应用程序哪些描述符可读、可写或出错了（即产生事件Event）。
**kqueue 支持的event很多，文件句柄事件，信号，异步io事件，子进程状态事件，支持微秒的计时器事件等。**

```c
struct kevent
{
uintptr_t  ident;   /* identifier for this event */ 事件标识
short  filter;     /* filter for event */ 监听事件的类型，如EVFILT_READ，EVFILT_WRITE，EVFILT_TIMER等
u_short  flags;   /* action flags for kqueue */事件操作类型，如EV_ADD，EV_ENABLE，EV_DELETE等
u_int  fflags;       /* filter flag value */
intptr_t  data;      /* filter data value */
void  *udata;       /* opaque user data identifier */可携带的任意用户数据
};

// 初始化kqueue
int kq = kqueue(); // kqueue对象
struct kevent events_changes[KQUEUE_SIZE]; // kevent 返回监听结果
// kevent(ident, filter, flags, fflags, data, udata) 一次只能监听一个事情
struct kevent event;

// 检测标准输入流STDIN
EV_SET(&event, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, NULL);
kevent(kq, &event, 1, NULL, 0, NULL);

// 当serv_sock有数据可以读的时候，kqueue会触发
EV_SET(&event, serv_sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
// pass data to kqueue
kevent(kq, &event, 1, NULL, 0, NULL);
```

