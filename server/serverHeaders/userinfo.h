#ifndef _USERINFO_H_
#define _USERINFO_H_

#include <string.h>

//用户信息结构体，用于传递客户的信息
typedef struct usrinfo
{
    //账号为6位数字
    char id[7];
    //密码为6-13个字符
    char passwd[14];
    //昵称最长为15个字节
    char name[16];
}usrinfo;

void strcpy_usrinfo(usrinfo *dest, const usrinfo *src);
void init_usrinfo(usrinfo *dest, const char *id, const char *passwd, const char *name);

#endif // end of userinfo.h