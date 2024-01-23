#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <ctype.h>
#include "../wrap.h"

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

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(lfd, &readfds);
    fd_set tmpfds;

    int maxfd = lfd;
    int nready;
    int cfd;

    while (1)
    {
        tmpfds = readfds;
        nready = select(maxfd + 1, &tmpfds, NULL, NULL, NULL);
        if (nready <= 0)
        {
            if (errno == EINTR)
                continue;
            else
                break;
        }

        if (FD_ISSET(lfd, &tmpfds))
        {
            cfd = Accept(lfd, NULL, NULL);
            FD_SET(cfd, &readfds);
            if (cfd > maxfd)
                maxfd = cfd;
            nready -= 1;
        }

        if (nready < 1)
            continue;

        char buf[1024] = {0};
        for (int fdi = lfd + 1; fdi <= maxfd; fdi += 1)
        {
            if (FD_ISSET(fdi, &tmpfds))
            {
                memset(buf, 0, strlen(buf));
                if (Read(fdi, buf, sizeof(buf)) <= 0)
                {
                    close(fdi);
                    FD_CLR(fdi, &readfds);
                    continue;
                }

                for (int i = 0; i < strlen(buf); i += 1) {
                    buf[i] = toupper(buf[i]);
                }

                Write(fdi, buf, strlen(buf));

                if(--nready < 1) break;
            }
        }
    }

    close(lfd);
    return 0;
}
