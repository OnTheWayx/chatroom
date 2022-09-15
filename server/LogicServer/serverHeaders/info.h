//客户端选择的操作以及所需传递的信息
#ifndef _INFO_H_
#define _INFO_H_

#include "userinfo.h"
#include "clientoptions.h"

#define BUF_MAXSIZE 128

#define CLIENT_INFO_SIZE 176

// 176
typedef struct info
{
    enum option opt;
    struct usrinfo usr;
    //消息正文
    char text[BUF_MAXSIZE];
    unsigned char _fill[4];
}info;

// 176字节
typedef struct downfileinfo
{
    // 标志位
    enum option opt;
    // 文件大小
    long filesize;

    // 填补至176字节
    unsigned char _fill[CLIENT_INFO_SIZE - sizeof(long) - sizeof(enum option) - 4];
}downfileinfo;

#endif  // end of info.h