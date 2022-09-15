#ifndef _FILESERVER_H_
#define _FILESERVER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>

#include "protocol.h"
#include "fileclientoption.h"

#define FAILURE -1
#define SUCCESS 0

#define BUF_SIZE 1024

typedef struct fileserver_t
{
    // 服务器的套接字描述符
    int sockfd;

    // 服务器的端口与ip
    unsigned short port;

    // 服务器是否关闭标志
    short shutdown;

    const char *ip;
}fileserver_t;

/**
 * 初始化服务器基本参数
 * fileserver : 服务器基本参数结构体地址
 * ip : 以字符串形式存放的ip地址的首地址
 * port : 监听端口
 * 返回值 : 初始化成功返回0，失败返回-1
 */
int InitFileServer(fileserver_t *fileserver, const char *ip, unsigned short port);

/**
 * 做好监听连接前的初始化工作
 * fileserver : 服务器基本参数结构体地址
 * 返回值 : 初始化成功返回0，失败返回-1
 */
int StartReadyFileServer(fileserver_t *fileserver);

/**
 * 启动服务进程
 * fileserver : 服务器基本参数结构体地址
 * 无返回值
 */
void Start(fileserver_t *fileserver);

/**
 * 获取套接字文件描述符
 * 无参数
 * 返回值 : 创建的套接字描述符
 */
int GetSocketfd();

/**
 * 客户端服务进程
 */
void ClientService(fileserver_t *fileserver, int fd);

/**
 * 向客户端发送目标文件
 */
void SendFile(info *recvinfo);

#endif // end of fileserver.h