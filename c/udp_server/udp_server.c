#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
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
    int cfd = Socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    Bind(cfd, (struct sockaddr *)&serv, sizeof(serv));

    struct sockaddr_in clit;
    char buf[1024];
    socklen_t clit_len;

    while (1)
    {
        memset(buf, 0, strlen(buf));
        recvfrom(cfd, buf, sizeof(buf), 0, (struct sockaddr *)&clit, &clit_len);
        printf("client %d visited\n", clit.sin_addr.s_addr);

        for (int i = 0; i < strlen(buf); i += 1)
        {
            buf[i] = toupper(buf[i]);
        }

        sendto(cfd, buf, strlen(buf), 0, (struct sockaddr *)&clit, clit_len);
    }

    return 0;
}
