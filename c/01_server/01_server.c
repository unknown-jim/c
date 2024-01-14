#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(const int argc, const char** argv)
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd < 0)
    {
        perror("socket:");
        return -1;
    }

    struct sockaddr_in serv;
    bzero(&serv, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(lfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        perror("bind:");
        return -1;
    }

    if (listen(lfd, 128) < 0)
    {
        perror("listen:");
        return -1;
    }

    while(1)
    {
        printf("waiting connection\n");
        int cfd = accept(lfd, NULL, NULL);

        char buffer[1024];

        while(1)
        {
            memset(buffer, 0, sizeof(buffer));
            read(cfd, buffer, sizeof(buffer));
            printf("\nclient: %s", buffer);
            if (strncmp(buffer, "quiting", 7) == 0)
            {
                printf("conversation over\n");
                break;
            }

            printf("\nserver:");
            memset(buffer, 0, sizeof(buffer));
            scanf("%s", buffer);
            write(cfd, buffer, strlen(buffer));
        }

        close(cfd);
    }
    close(lfd);

    return 0;
}
