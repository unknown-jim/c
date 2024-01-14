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
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if(cfd < 0)
    {
        perror("socket:");
        return -1;
    }

    ;

    return 0;
}
