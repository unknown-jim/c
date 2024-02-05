#ifndef _PUB_H
#define _PUB_H
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <ctype.h>
char *get_file_type(char *name);
int get_line(int sock, char *buf, int size);
int hexit(char c);                                         // 16����ת10����
void strencode(char *to, size_t tosize, const char *from); // ����
void strdecode(char *to, char *from);                      // ����
#endif
