#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <ctype.h>
#include "../../wrap.h"

int main(const int argc, const char **argv)
{
    int lfd = Socket(AF_INET, SOCK_STREAM, 0);

    int optval = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    Bind(lfd, (struct sockaddr *)&serv, sizeof(serv));
    Listen(lfd, 128);

    int epfd = epoll_create(1);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

    struct epoll_event events[128];
    int maxep = 1;
    char buf[64];
    while (1)
    {
        int nready = epoll_wait(epfd, events, maxep, -1);
        if (nready < 0)
        {
            perror("epoll_wait:");
            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < nready; i += 1)
        {
            if (events[i].data.fd == lfd)
            {
                ev.data.fd = Accept(lfd, NULL, NULL);
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);

                int flag = fcntl(ev.data.fd, F_GETFL) | O_NONBLOCK;
                fcntl(ev.data.fd, F_SETFL, flag);

                maxep += 1;
                continue;
            }

            while (1)
            {
                memset(buf, 0, strlen(buf));
                int n = Read(events[i].data.fd, buf, sizeof(buf));

                if (n == -1)
                {
                    printf("read over\n");
                    break;
                }

                if (n <= 0)
                {
                    close(events[i].data.fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    maxep -= 1;

                    printf("read error or client closed\n");

                    break;
                }

                for (int j = 0; j < strlen(buf); j += 1)
                {
                    buf[j] = toupper(buf[j]);
                }
                Write(events[i].data.fd, buf, strlen(buf));
            }
        }
    }

    close(epfd);
    close(lfd);

    return 0;
}
