#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include "../wrap.h"

int main(const int argc, const char **argv)
{
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    if (inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr.s_addr) != 1)
    {
        perror("inet_pton: ");
        return -1;
    }

    char buf[1024];

    while (1)
    {
        int cfd = Socket(AF_INET, SOCK_DGRAM, 0);
        while (1)
        {
            memset(buf, 0, strlen(buf));
            printf("client: ");
            scanf("%s", buf);

            if (strlen(buf) == 7 && strncmp(buf, "quiting", 7) == 0)
            {
                Close(cfd);
                break;
            }

            sendto(cfd, buf, strlen(buf), 0, (struct sockaddr *)&serv, sizeof(serv));

            memset(buf, 0, strlen(buf));

            Read(cfd, buf, sizeof(buf));
            printf("server: %s\n", buf);
        }

        printf("continue?[y/n]\n");
        memset(buf, 0, strlen(buf));
        scanf("%s", buf);
        while (strlen(buf) != 1 || (buf[0] != 'y' && buf[0] != 'n'))
        {
            printf("input 'y' or 'n'\n");
            memset(buf, 0, sizeof(buf));
            scanf("%s", buf);
        }

        if (buf[0] == 'n')
            break;
    }

    return 0;
}
