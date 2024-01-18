#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "../wrap.h"

    int main(const int argc, const char **argv)
{
    int lfd = tcp4bind(8888, NULL);

    Listen(lfd, 128);

    int i = 0;
    struct sockaddr_in clt;
    char sIP[16];
    socklen_t len;
    while (1)
    {
        len = sizeof(clt);
        int cfd = Accept(lfd, (struct sockaddr *)&clt, &len);

        int pid = fork();

        if (pid < 0)
        {
            perror("fork:");
            return -1;
        }
        else if (pid > 0)
        {
            close(cfd);
            i += 1;
        }
        else
        {
            close(lfd);
            char buffer[1024] = {0};

            while (1)
            {
                Read(cfd, buffer, sizeof(buffer));

                if (strlen(buffer) == 7 && strncmp(buffer, "quiting", 7) == 0)
                {
                    printf("client%d: %s\n", i, buffer);
                    break;
                }

                printf("%s calling\n", inet_ntop(AF_INET, &clt.sin_addr.s_addr, sIP, sizeof(sIP)));

                for (int j = 0; j < strlen(buffer); j += 1)
                {
                    buffer[j] = toupper(buffer[j]);
                }

                Write(cfd, buffer, strlen(buffer));
                memset(buffer, 0, strlen(buffer));
            }

            printf("conversation %d over\n", i);
            close(cfd);
            exit(0);
        }
    }

    close(lfd);

    return 0;
}
