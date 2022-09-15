#include <stdio.h>
enum option
{
    ONE
};

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


typedef struct info
{
    enum option opt;
    struct usrinfo usr;
    //消息正文
    char text[128];
    unsigned char _fill[4];
}info;

int main(void)
{
    printf("%d\n", sizeof(info));


    return 0;
}