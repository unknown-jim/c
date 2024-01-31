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
    if (lfd < 0)
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

    while (1)
    {
        printf("waiting connection\n");
        int cfd = accept(lfd, NULL, NULL);

        char buffer[1024];

        while (1)
        {
            memset(buffer, 0, sizeof(buffer));
            read(cfd, buffer, sizeof(buffer));
            printf("\nclient: %s", buffer);
            if (strncmp(buffer, "quiting", 8) == 0)
            {
                printf("\nconversation over\n");
                memset(buffer, 0, sizeof(buffer));
                break;
            }

            printf("\nserver: ");
            memset(buffer, 0, sizeof(buffer));
            scanf("%s", buffer);
            write(cfd, buffer, strlen(buffer));
        }

        close(cfd);

        printf("\ncontinue?[y/n]\ninput: ");
        scanf("%s", buffer);
        while (strlen(buffer) != 1 || (buffer[0] != 'y' && buffer[0] != 'n'))
        {
            printf("input 'y' or 'n'\ninput: ");
            memset(buffer, 0, sizeof(buffer));
            scanf("%s", buffer);
        }

        if (buffer[0] == 'n')
            break;
    }
    close(lfd);

    return 0;
}
