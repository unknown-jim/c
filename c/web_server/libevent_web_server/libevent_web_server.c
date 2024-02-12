#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <event.h>
#include <event2/listener.h>
#include "../../wrap.h"
#include "../../pub.h"

#define _WORK_DIR_ "%s/webpath"
#define _DIR_PREFIX_FILE_ "html/dir_header.html"
#define _DIR_TAIL_FILE_ "html/dir_tail.html"

void copy_header(struct bufferevent *bev, int op, char *msg, char *fileType, int fileSize)
{
    char buf[4096] = {0};
    sprintf(buf, "HTTP/1.1 %d %s\r\n", op, msg);
    sprintf(buf + strlen(buf), "Content-Type: %s\r\n", fileType);

    // 如果明知代发送文件大小，则告知客户端
    // 客户端接收指定量数据后停止接收
    if (fileSize > 0)
    {
        sprintf(buf + strlen(buf), "Content-Length: %d\r\n", fileSize);
    }

    strcat(buf, "\r\n");
    bufferevent_write(bev, buf, strlen(buf));
}

void copy_file(struct bufferevent *bev, char *path)
{
    int fd = open(path, O_RDONLY);
    char buf[1024] = {0};

    int n;
    while ((n = Read(fd, buf, sizeof(buf))) > 0)
    {
        bufferevent_write(bev, buf, n);
    }
    close(fd);
}

// 发送目录文件
void sendDir(struct bufferevent *bev, char *path)
{
    // 发送时，实际上是发送一个拼接的html文件
    // 先发送一个html文件头
    copy_file(bev, _DIR_PREFIX_FILE_);

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        perror("opendir: ");
        return;
    }

    char buf[1024];
    struct dirent *dirent = NULL;

    // 读取目录文件
    while (dirent = readdir(dir))
    {
        struct stat st;
        stat(dirent->d_name, &st);
        memset(buf, 0, strlen(buf));
        if (dirent->d_type == DT_DIR)
        {
            // 如果是目录文件，则超链接末尾需要加上‘/’
            sprintf(buf, "<li><a href='%s/'>%32s</a>    %8ld</li>", dirent->d_name, dirent->d_name, st.st_size);
        }
        else if (dirent->d_type == DT_REG)
        {
            sprintf(buf, "<li><a href='%s'>%32s</a>     %8ld</li>", dirent->d_name, dirent->d_name, st.st_size);
        }
        bufferevent_write(bev, buf, strlen(buf));
    }

    closedir(dir);
    copy_file(bev, _DIR_TAIL_FILE_);
}

// 访问响应函数
void http_request(struct bufferevent *bev, char *path)
{
    // 解析汉字路径
    strdecode(path, path);

    // 识别是否是访问主目录
    if (strcmp(path, "/") == 0 || strcmp(path, "/.") == 0)
        path = "./";
    else
        path += 1;

    // 获取该路径的文件的信息
    struct stat st;
    if (stat(path, &st) < 0)
    {
        perror("stat: ");
        copy_header(bev, 404, "NOT FOUND", get_file_type("error.html"), -1);
        copy_file(bev, "error.html");
    }

    if (S_ISDIR(st.st_mode))
    {
        copy_header(bev, 200, "OK", get_file_type("w.html"), st.st_size);
        sendDir(bev, path);
    }

    else if (S_ISREG(st.st_mode))
    {
        copy_header(bev, 200, "OK", get_file_type(path), st.st_size);
        copy_file(bev, path);
    }
}

// 读事件回调函数
void readcb(struct bufferevent *bev, void *arg)
{
    char buf[256] = {0};
    char method[10], path[256], protocal[10];

    int n = bufferevent_read(bev, buf, sizeof(buf));
    if (n <= 0)
        return;
    write(STDOUT_FILENO, buf, n);

    // 解析出访问的目的、请求资源的路径、协议
    sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", method, path, protocal);

    if (strcasecmp(method, "get") != 0)
        return;

    // 防止粘包
    while (bufferevent_read(bev, buf, sizeof(buf)) > 0)
        ;

    // 调用响应函数
    http_request(bev, path);
}

// 有特殊事件时的回调函数
void beventcb(struct bufferevent *bev, short what, void *arg)
{
    // 客户端退出
    if (what & BEV_EVENT_EOF)
    {
        printf("a client left\n");
        bufferevent_free(bev);
    }
    // 出错
    else if (what & BEV_EVENT_ERROR)
    {
        printf("err to client closed\n");
        bufferevent_free(bev);
    }
    else if (what & BEV_EVENT_CONNECTED)
    { // 连接成功
        printf("client connect ok\n");
    }
}

// 链接监听器的回调函数
void conncfd(struct evconnlistener *listener, evutil_socket_t cfd, struct sockaddr *clit, int sockLen, void *arg)
{
    struct bufferevent *bev = bufferevent_socket_new((struct event_base *)arg, cfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, readcb, NULL, beventcb, arg);
    bufferevent_enable(bev, EV_READ);
}

int main(const int argc, const char **argv)
{
    // 更改进程运行路径
    char path[256] = {0};
    sprintf(path, "%s%s", getenv("HOME"), "/Study/webpath");
    chdir(path);

    // 创建event树
    struct event_base *base = event_base_new();
    if (!base)
    {
        perror("event_base_new: ");
        return -1;
    }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8888);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    // 创建链接监听器
    struct evconnlistener *listener = evconnlistener_new_bind(base, conncfd, base, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr *)&serv, sizeof(serv));

    // 进入event事件循环
    event_base_dispatch(base);

    // 释放event树和链接监听器
    event_base_free(base);
    evconnlistener_free(listener);

    return 0;
}
