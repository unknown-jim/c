#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../../wrap.h"
#include "../../pub.h"

//发送http协议头
void send_head(int cfd, char *code, char *msg, char *fileType, int len)
{
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %s %s\r\n", code, msg);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", fileType);

    //如果知道文件具体大小，就告知client
    //client会在收到通知的文件尺寸后关闭连接
    //如果不知道，client不会关闭连接
    if (len > 0)
    {
        sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    }
    strcat(buf, "\r\n");
    // 必须用strlen，否则会发送多余的'\0'
    Write(cfd, buf, strlen(buf));
}

//发送文件体
void send_file(int cfd, char *fileName)
{
    int fd = open(fileName, O_RDONLY);
    if (fd < 0)
    {
        perror("open: ");
        return;
    }

    char buf[1024];
    //循环读和发送，直到文件发完
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
    //接收http协议请求消息的第一行
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

    // 从请求消息的第一行解析出请求的类型、文件名、协议
    sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", reqType, fileName, protocal);

    char *pFile = fileName;
    //如果文件名为空，则是访问主目录
    if (strlen(fileName) <= 1)
    {
        strcpy(pFile, "./");
    }
    //否则从文件名里去掉第一个‘/’
    else
    {
        pFile = fileName + 1;
    }

    //将传来带%的汉字编码字符串解析为汉字编码
    //非汉字编码保持不变
    strdecode(pFile, pFile);

    //将请求消息读完避免粘包
    while (Readline(cfd, buf, sizeof(buf)) > 0)
        ;

    //根据文件名获得文件信息
    struct stat st;
    //如果获取信息失败或没有找到文件则发送错误页面
    if (stat(pFile, &st) < 0)
    {  
        printf("file not exist\n");
        send_head(cfd, "404", "NOT FOUND", get_file_type(".html"), 0);
        send_file(cfd, "error.html");
    }
    else
    {
        //请求的如果是普通文件
        //直接发送协议头和文件体
        if (S_ISREG(st.st_mode))
        {
            printf("file exist\n");
            send_head(cfd, "200", "OK", get_file_type(pFile), st.st_size);
            send_file(cfd, pFile);
        }
        //请求的如果是目录文件
        //先发送协议头和.html文件的开头部分
        //然后扫描目录，将目录内的文件作为超链接依次发送
        //最后发送.html文件的结尾部分
        else if (S_ISDIR(st.st_mode))
        {
            printf("dir exist\n");
            char buf[1024];
            send_head(cfd, "200", "OK", get_file_type(".html"), 0);
            send_file(cfd, "html/dir_header.html");

            struct dirent **nameList;

            // vscode内 scandir需要指定c标准
            int n = scandir(pFile, &nameList, NULL, alphasort);
            if (n < 0)
            {
                //如果目录扫描失败，则关闭客户端连接并且下树
                perror("scandir: ");
                close(cfd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
            }
            else
            {
                while (n--)
                {
                    memset(buf, 0, strlen(buf));
                    //如果是目录文件，则需要在超链接最后加上‘/’
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

    //当server发送数据时，client关闭连接
    //server进程会收到SIGPIPE导致退出
    //改变进程收到SIGPIPE信号的行为
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, NULL);

    //signal()函数只能改变一次信号的行为，之后又会恢复原来的行为
    //handler函数内再次执行signal()函数前，再次收到相同信号，可能会造成进程意外退出
    // signal(SIGPIPE, SIG_IGN);

    // 创建tcp socket、设置端口复用并且bind
    int lfd = tcp4bind(8888, NULL);

    //创建epoll树
    int epfd = epoll_create(1024);
    if (epfd < 0)
    {
        perror("epoll_create: ");
        close(lfd);
        return -1;
    }

    //将lfd上树
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
        //等待时间返回
        nready = epoll_wait(epfd, events, 1024, -1);
        if (nready < 0)
        {
            //如果epoll_wait意外中断，则继续等待
            if (errno == EINTR)
                continue;
            //如果出错，退出循环
            perror("epoll_wait: ");
            break;
        }

        for (int i = 0; i < nready; i += 1)
        {
            //如果监听到lfd事件
            //将新接收的客户端连接上树
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
            //否则，响应客户端传来的请求
            else
            {
                http_respond(events[i].data.fd, epfd);
            }
        }
    }

    return 0;
}
