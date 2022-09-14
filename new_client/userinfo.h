// 存放用户登录后的信息
#ifndef _USERINFO_H_
#define _USERINFO_H_

//存放客户端自身的信息
struct usrinfo
{
    //账号为6位数da字
    char id[7];
    //密码为6-13个字符
    char passwd[14];
    //昵称最长为15个字节
    char name[16];
};

#endif  // end of userinfo.h