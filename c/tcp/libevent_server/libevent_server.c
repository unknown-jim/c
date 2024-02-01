#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <event2/event.h>
#include "../../wrap.h"

void comm(evutil_socket_t cfd, short evevts, void *arg)
{
    char buf[1024];

    int n = Read(cfd, buf, sizeof(buf));
    if (n == 0)
    {
        close(cfd);
        event_free(*(struct event **)arg);
        free((struct event **)arg);
        printf("client quited\n");
        return;
    }

    printf("client visited\n");
    for (int i = 0; i < n; i++)
    {
        buf[i] = toupper(buf[i]);
    }

    Write(cfd, buf, n);
}

void conncfd(evutil_socket_t lfd, short evevts, void *arg)
{
    int cfd = Accept(lfd, NULL, NULL);
    if (cfd < 0)
        return;

    struct event **evp = (struct event **)malloc(sizeof(struct event *));
    *evp = event_new((struct event_base *)arg, cfd, EV_READ | EV_PERSIST, comm, evp);
    if (*evp == NULL)
    {
        perror("event_new: ");
        return;
    }

    event_add(*evp, NULL);
}

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

    struct event_base *base = event_base_new();
    if (base == NULL)
    {
        perror("event_base_new: ");
        return -1;
    }

    struct event *ev = event_new(base, lfd, EV_READ | EV_PERSIST, conncfd, base);
    if (ev == NULL)
    {
        perror("event_new: ");
        return -1;
    }

    event_add(ev, NULL);

    event_base_dispatch(base);
    event_free(ev);
    event_base_free(base);

    return 0;
}
