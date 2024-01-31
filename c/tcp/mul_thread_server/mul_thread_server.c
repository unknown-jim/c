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
#include <pthread.h>
#include "../../wrap.h"

void *thread_worker(void *arg)
{
    int cfd = *(int *)arg;
    char buff[1024] = {0};

    while (1)
    {
        if (Read(cfd, buff, sizeof(buff)) <= 0)
        {
            printf("conversation over or read error\n");
            break;
        }

        printf("client%d :%s\n", cfd, buff);

        if (strlen(buff) == 7 && strncmp(buff, "quiting", 7) == 0)
            break;

        for (int i = 0; i < strlen(buff); i += 1)
        {
            buff[i] = toupper(buff[i]);
        }
        Write(cfd, buff, strlen(buff));
        memset(buff, 0, strlen(buff));
    }

    close(cfd);
    *(int *)arg = -1;
    printf("conversation%d over\n", cfd);
    pthread_exit(NULL);
}

int main(const int argc, const char **argv)
{
    int lfd = tcp4bind(8888, NULL);

    Listen(lfd, 128);

    struct sockaddr_in client;
    socklen_t len;
    pthread_t threadID;
    int cfd[128];

    for (int i = 0; i < 128; i += 1)
    {
        cfd[i] = -1;
    }

    int temp_cfd;
    while (1)
    {
        temp_cfd = Accept(lfd, (struct sockaddr *)&client, &len);
        int i = 0;
        while (i < 128 && cfd[i] != -1)
            i += 1;
        if (i == 128)
        {
            close(temp_cfd);
            continue;
        }
        cfd[i] = temp_cfd;

        pthread_create(&threadID, NULL, thread_worker, (void *)&cfd[i]);
        pthread_detach(threadID);
    }

    close(lfd);

    return 0;
}
