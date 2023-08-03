//
// Created by Shawn Zhao on 2023/8/3.
//

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define BUF_SIZE 1024
#define KQUEUE_SIZE 50
#define TIMEOUT 5
void setnonblockingmode(int fd) { // 更改文件的操作类型
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag|O_NONBLOCK);
}

void error_handling(char *buf) {
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}


int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t adr_sz;
    int str_len, i;
    char buf[BUF_SIZE];

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
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

    setnonblockingmode(serv_sock);
    // 设置监听时间
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_sec = TIMEOUT; // 设置收集结果的最长时间为5s，否则为超时

    while (1) {
        int event_cnt = kevent(kq, NULL, 0, events_changes, KQUEUE_SIZE, NULL);
        if (event_cnt < 0) {
            error_handling("kevent error");
            break;
        }
        else if (event_cnt == 0) {
            printf("kevent 超时\n");
            continue;
        }
        else { // event_cnt > 0
            for (int i = 0; i < event_cnt; ++i) {
                if (events_changes[i].ident == serv_sock) {
                    // 建立新的连接
                    adr_sz = sizeof (clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
                    setnonblockingmode(clnt_sock);
                    EV_SET(&event, clnt_sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
                    kevent(kq, &event, 1, NULL, 0, NULL);
                    printf("connected client: %d \n", clnt_sock);
                }
                else {
                    int count = 0;
                    while (1) {
                        // mast read from
                        str_len = read(events_changes[i].ident, buf, BUF_SIZE);
                        if (str_len == 0) {
                            // close connect
                            // 设置删除数据
                            EV_SET(&event, events_changes[i].ident, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                            kevent(kq, &event, 1, NULL, 0, NULL);

                            close((int) events_changes[i].ident); // 关闭
                            printf("closed client: %d \n", events_changes[i].ident);
                            break;
                        } else if (str_len < 0) { // 针对于非阻塞方式，此时是没有数据可以用来读取的。
                            if (errno == EAGAIN) // 说明当前没有数据，可以退出循环
                                break;
                        } else {
                            write(events_changes[i].ident, buf, str_len); //echo!
                        }
                    }
                }
            }
        }
    }
    close(serv_sock);
    close(kq);
    return 0;
}
