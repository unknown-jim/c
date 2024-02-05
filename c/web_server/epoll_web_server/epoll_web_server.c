#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../../wrap.h"
#include "../../pub.h"

void send_head(int cfd, char *code, char *msg, char *fileType, int len)
{
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %s %s\r\n", code, msg);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", fileType);

    if (len > 0)
    {
        sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    }
    strcat(buf, "\r\n");
    // 必须用strlen，否则会发送多余的'\0'
    Write(cfd, buf, strlen(buf));
}

void send_file(int cfd, char *fileName)
{
    int fd = open(fileName, O_RDONLY);
    if (fd < 0)
    {
        perror("open: ");
        return;
    }

    char buf[1024];
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        if (Read(fd, buf, sizeof(buf)) <= 0)
            break;
        else
            Write(cfd, buf, sizeof(buf));
    }
}

void http_respond(int cfd, int epfd)
{
    char buf[1024] = {0};
    if (Readline(cfd, buf, sizeof(buf)) <= 0)
    {
        close(cfd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        return;
    }
    printf("buf = %s\n", buf);

    char reqType[16] = {0};
    char fileName[255] = {0};
    char protocal[16] = {0};

    sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", reqType, fileName, protocal);

    char *pFile = fileName;
    if (strlen(fileName) <= 1)
    {
        strcpy(pFile, "./");
    }
    else
    {
        pFile = fileName + 1;
    }

    strdecode(pFile, pFile);

    while (Readline(cfd, buf, sizeof(buf)) > 0)
        ;

    struct stat st;
    if (stat(pFile, &st) < 0)
    {
        printf("file not exist\n");
        send_head(cfd, "404", "NOT FOUND", get_file_type(".html"), 0);
        send_file(cfd, "error.html");
    }
    else
    {
        if (S_ISREG(st.st_mode))
        {
            printf("file exist\n");
            send_head(cfd, "200", "OK", get_file_type(pFile), st.st_size);
            send_file(cfd, pFile);
        }
        if (S_ISDIR(st.st_mode))
        {
            printf("dir exist\n");
            char buf[1024];
            send_head(cfd, "200", "OK", get_file_type(".html"), 0);
            send_file(cfd, "html/dir_header.html");

            struct dirent **nameList;

            // scandir需要指定c标准
            int n = scandir(pFile, &nameList, NULL, alphasort);
            if (n < 0)
            {
                perror("scandir: ");
                close(cfd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
            }
            else
            {
                while (n--)
                {
                    memset(buf, 0, strlen(buf));
                    if (nameList[n]->d_type == DT_DIR)
                        sprintf(buf, "<li><a href=%s/> %s </a></li>", nameList[n]->d_name, nameList[n]->d_name);
                    else
                        sprintf(buf, "<li><a href=%s> %s </a></li>", nameList[n]->d_name, nameList[n]->d_name);
                    free(nameList[n]);
                    Write(cfd, buf, strlen(buf));
                }
                free(nameList);
            }

            send_file(cfd, "html/dir_tail.html");
        }
    }
}

int main(const int argc, const char **argv)
{
    // 改变进程工作目录
    char path[256] = {0};
    sprintf(path, "%s%s", getenv("HOME"), "/Study/webpath");
    chdir(path);

    // 创建tcp socket、设置端口复用并且bind
    int lfd = tcp4bind(8888, NULL);

    int epfd = epoll_create(1024);
    if (epfd < 0)
    {
        perror("epoll_create: ");
        close(lfd);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = lfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

    Listen(lfd, 1);

    int nready;
    int cfd;
    struct epoll_event events[1024];
    while (1)
    {
        nready = epoll_wait(epfd, events, 1024, -1);
        if (nready < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wait: ");
            break;
        }

        for (int i = 0; i < nready; i += 1)
        {
            if (events[i].data.fd == lfd)
            {
                cfd = Accept(lfd, NULL, NULL);

                // 将cfd设置为非阻塞
                int flag = fcntl(cfd, F_GETFL);
                flag |= O_NONBLOCK;
                fcntl(cfd, F_SETFL, flag);

                // 将cfd上epoll树
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            }
            else
            {
                http_respond(events[i].data.fd, epfd);
            }
        }
    }

    return 0;
}
