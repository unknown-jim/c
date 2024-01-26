#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <time.h>
#include "../wrap.h"

#define MAX_EVENTS 1024          // 通信文件描述符最大监听数量
const static int BUFSIZE = 1024; // 每个连接缓存区大小

// epoll_wait 传回的信息
struct events_arg
{
    int fd;
    int events;
    struct events_arg *arg;
    void (*call_back)(struct events_arg *ev);
    char status;
    char *buf;
    int buf_len;
    long last_time;
};

void recvData(struct events_arg *ev);

static struct events_arg listening[MAX_EVENTS + 1];
static int epfd; // epoll根节点

// 初始化通信连接 epoll_wait 传回的信息
void event_set(int fd, struct events_arg *ev, void (*call_back)(struct events_arg *ev))
{
    ev->fd = fd;
    ev->arg = ev;
    ev->call_back = call_back;
    ev->last_time = time(NULL);
}

// 从epoll树中删除通信连接节点
void event_del(struct events_arg *ev)
{
    if (!ev->status)
        return;

    ev->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, NULL);
}

// 将通信连接插入epoll树，或者修改通信连接属性
void event_add(int events, struct events_arg *ev)
{
    struct epoll_event epev;
    epev.events = events;
    epev.data.ptr = (void *)ev;
    int op;

    if (ev->status == 1)
    {
        op = EPOLL_CTL_MOD;
    }
    else
    {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if (epoll_ctl(epfd, op, ev->fd, &epev) < 0)
    {
        perror("epoll_ctl :");
        event_del(ev);
        close(ev->fd);
        free(ev->buf);
    }
}

// 读缓存区可用时的回调函数
void sendData(struct events_arg *ev)
{
    for (int i = 0; i < ev->buf_len; i += 1)
    {
        ev->buf[i] = toupper(ev->buf[i]);
    }

    event_del(ev);
    if (Write(ev->fd, ev->buf, ev->buf_len) > 0)
    {                                    // 从红黑树g_efd中移除
        event_set(ev->fd, ev, recvData); // 将该fd的 回调函数改为 recvdata
        event_add(EPOLLIN, ev);
    }
    else
    {
        close(ev->fd);
        free(ev->buf);
        perror("send :");
    }
}

// cfd发生改变时的回调函数
void recvData(struct events_arg *ev)
{
    memset(ev->buf, 0, ev->buf_len);
    int len = Read(ev->fd, ev->buf, BUFSIZE);

    event_del(ev);

    if (len > 0)
    {
        ev->buf_len = len;
        ev->buf[len] = '\0';
        printf("C[%d] visited\n", ev->fd);

        event_set(ev->fd, ev, sendData);
        event_add(EPOLLOUT, ev);
    }
    else if (len == 0)
    {
        close(ev->fd);
        free(ev->buf);
        printf("[fd=%d], [pos:%ld], closed\n", ev->fd, ev - listening);
    }
    else
    {
        close(ev->fd);
        free(ev->buf);
        printf("read [fd=%d] error[%d]:%s\n", ev->fd, errno, strerror(errno));
    }
}

// lfd发生改变时的回调函数
void acceptConn(struct events_arg *ev)
{
    struct sockaddr_in cin;
    socklen_t cinLen = sizeof(cin);

    int i = 0, cfd = Accept(ev->fd, (struct sockaddr *)&cin, &cinLen);

    for (; i < MAX_EVENTS && listening[i].status == 1; i += 1)
        ;

    if (i == MAX_EVENTS)
    {
        printf("max connect limitation[%d]\n", MAX_EVENTS);
        close(cfd);
        return;
    }

    int flag = fcntl(cfd, F_GETFL, 0);
    flag |= O_NONBLOCK;
    if (fcntl(cfd, F_SETFL, flag) < 0)
    {
        perror("fcntl_set :");
        close(cfd);
        return;
    }

    event_set(cfd, &listening[i], recvData);
    listening[i].buf = malloc(BUFSIZE);
    listening[i].buf_len = 0;
    event_add(EPOLLIN, &listening[i]);

    printf("new connection [%s][time:%ld], [pos:%ld]\n", inet_ntoa(cin.sin_addr), listening[i].last_time, listening[i].arg - listening);
}

// 初始化监听文件描述符
int initListenSock()
{
    int lfd = Socket(AF_INET, SOCK_STREAM, 0);

    int optval = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    event_set(lfd, &listening[MAX_EVENTS], acceptConn);
    event_add(EPOLLIN, &listening[MAX_EVENTS]);

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    Bind(lfd, (struct sockaddr *)&serv, sizeof(serv));

    Listen(lfd, 128);

    return lfd;
}

int main(const int argc, const char **argv)
{
    epfd = epoll_create(1);
    if (epfd < 0)
    {
        perror("epoll_create error:");
        return -1;
    }

    int lfd = initListenSock();

    struct epoll_event events[MAX_EVENTS + 1];

    int check_pos = 0, i;

    while (1)
    {
        // 每次检查100个连接有没有超时
        long now = time(NULL);
        for (i = 0; i < 100; i += 1, check_pos += 1)
        {
            if (check_pos == MAX_EVENTS)
                check_pos = 0;

            if (listening[check_pos].status != 1)
            {
                // i -= 1;
                continue;
            }

            long duration = now - listening[check_pos].last_time;

            if (duration > 30) // 超过10s就超时
            {
                close(listening[check_pos].fd);
                free(listening[check_pos].buf);
                event_del(&listening[check_pos]);
                printf("[fd=%d] timeout\n", listening[check_pos].fd);
            }
        }

        // 监听
        int nready = epoll_wait(epfd, events, MAX_EVENTS + 1, 1000);
        if (nready < 0)
        {
            perror("epoll_wait :");
            break;
        }

        for (int i = 0; i < nready; i += 1)
        {
            struct events_arg *ev = events[i].data.ptr;
            ev->call_back(ev);
        }
    }

    close(lfd);
    close(epfd);
    return 0;
}
