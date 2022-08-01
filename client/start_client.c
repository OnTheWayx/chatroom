#include "client.h"

int main(int argc, char const *argv[])
{
    //用于临时保存所调用函数返回值来判断是否发生了错误
    int ret;

    //开始运行时动画
    StartAnimation();
    //连接至服务器
    ret = ConnectServer();
    if (SUCCESS != ret)
    {
        printf("连接服务器失败...\n");

        return FAILURE;
    }
    
    //进行登陆或注册操作
    int choice = 0;

//如果登录失败或者注册成功，则返回此处登录
LOGIN:
    system("clear");
    LoReMenu();
    while (1)
    {
        scanf("%d", &choice);
        if (1 == choice || 2 == choice || 3 == choice)
        {
            break;
        }
        printf("输入有误，请重新输入！\n");
        printf("请选择：");
        getchar();
    }
    if (1 == choice)
    {
        //登录后载入自身信息
        ret = Login();
        if (FAILURE == ret)
        {
            printf("两秒后返回主页面！\n");
            sleep(2);
            goto LOGIN;
        }
    }
    else if (2 == choice)
    {
        //注册账号
        ret = Register();
        if (FAILURE == ret)
        {
            printf("注册失败\n两秒后返回主页面！\n");
            sleep(2);
        }
        else if (SUCCESS == ret)
        {
            printf("注册成功！\n");
            printf("按回车键返回主页面进行登录！\n");
            getchar();
            while (1)
            {
                if (10 == getchar())
                {
                    break;
                }
                sleep(1);
            }
        }
        //注册后返回i主页面进行登录
        goto LOGIN;
    }
    else if (3 == choice)
    {
        //找回密码
        RetrievePasswd();
        goto LOGIN;
    }
    while (1)
    {
        system("clear");
        Menu();
        printf("请选择：");
        choice = 0;
        scanf("%d", &choice);
        switch (choice)
        {
        case 1:
            //进入聊天室
            ChatHall();
            break;
        case 2:
            //查看在线用户
            ViewOnlineusers();
            break;
        case 3:
            //查看本地保存的聊天记录
            ViewChattingRecords();
            break;
        case 4:
            //向服务器上传文件
            UploadFiles();
            break;
        case 5:
            //从服务器下载文件
            DownloadFiles();
            break;
        case 6:
            //修改昵称
            UpdateName();
            break;
        case 7:
            //修改密码
            UpdatePasswd();
            break;
        case 8:
            //管理员操作
            admin();
            break;
        case 9:
            //退出程序
            Exit();
            break;
        default:
            break;
        }
        getchar();
    }

    return 0;
}
