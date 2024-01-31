#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(const int argc, const char **argv)
{
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    // serv.sin_addr.s_addr = htonl(INADDR_ANY);
    if (inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr.s_addr) != 1)
    {
        perror("inet_pton:");
        return -1;
    }

    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd < 0)
    {
        perror("socket:");
        return -1;
    }

    char buffer[1024] = {0};

    while (1)
    {

        if (connect(cfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
        {
            perror("connect:");
            return -1;
        }

        while (1)
        {
            memset(buffer, 0, sizeof(buffer));
            printf("\nclient:");
            scanf("%s", buffer);
            write(cfd, buffer, strlen(buffer));
            if (strncmp(buffer, "quiting", 8) == 0)
            {
                memset(buffer, 0, sizeof(buffer));
                break;
            }
            memset(buffer, 0, sizeof(buffer));
            read(cfd, buffer, sizeof(buffer));
            printf("server: %s\n", buffer);
        }

        close(cfd);

        printf("\ncontinue?[y/n]\n");
        scanf("%s", buffer);
        while (strlen(buffer) != 1 || (buffer[0] != 'y' && buffer[0] != 'n'))
        {
            printf("input 'y' or 'n'\n");
            memset(buffer, 0, sizeof(buffer));
            scanf("%s", buffer);
        }

        if (buffer[0] == 'n')
            break;
    }

    return 0;
}
