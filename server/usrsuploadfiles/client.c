#include "client.h"

// 客户端结构体，存放客户端基本参数
static struct client_t client;

//聊天大厅内容打印光标位置记录
static int cur = 12;

//存储聊天信息的文件流指针
FILE *ChatFp = NULL;

/**
 * @brief
 * 创建的两个工作线程
 * 1、负责接收消息
 * 2、负责监听是否被禁言或踢出聊天室
 */
static pthread_t tid[2] = {0};
// 确保同一时间只有一个recv函数接收消息
static sem_t sem_recv;

void StartAnimation()
{
    int i;

    system("resize -s 32 100");
    system("clear");
    printf("正在连接至服务器");
    fflush(stdout);
    for (i = 0; i < 6; i++)
    {
        //睡眠0.1 * 6秒
        usleep(100000);
        printf(".");
        fflush(stdout);
    }
    printf("\n");
}

int ConnectServer()
{
    // 初始化客户端基本参数
    client.client_shutdown = 0;
    client.disabledchat = 0;
    client.sockfd = 0;
    memset(&(client.myinfo), 0, sizeof(client.myinfo));

    //用于临时保存所调用函数的返回值判断是否发生了错误
    int ret;

    /**
     * 基于ipv4，传输协议为TCP，无其他附加协议
     */
    client.sockfd = socket(AF_INET, SOCK_STREAM, 0);

    //填充网络信息结构体，保存服务器信息
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);

    memset(&server_addr, 0, server_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8888);

    //连接至服务器
    ret = connect(client.sockfd, (struct sockaddr *)&server_addr, server_len);
    if (-1 == ret)
    {
        ERRLOG("connect");

        return FAILURE;
    }
    printf("连接服务器成功...\n");
    usleep(300000);

    return SUCCESS;
}

void LoReMenu()
{
    //设置窗口大小为32行100列
    system("resize -s 32 100");
    system("clear");
    printf("\033[3B\t\t\t\t --------------------------\n");
    printf("\t\t\t\t|        网络聊天室        |\n");
    printf("\t\t\t\t|                          |\n");
    printf("\t\t\t\t|   1.登录         2.注册  |\n");
    printf("\t\t\t\t|   3.忘记密码             |\n");
    printf("\t\t\t\t|                          |\n");
    printf("\t\t\t\t --------------------------\n");
    printf("请选择：");
}

//线程执行函数，监听客户端被禁言或踢出聊天室的消息
static void *ban_exit_handler(void *arg)
{
    int ret;

    struct info buf;

    // pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_detach(pthread_self());
    while (1)
    {
        // recv p操作
        sem_wait(&(sem_recv));
        memset(&buf, 0, sizeof(buf));
        ret = recv(client.sockfd, &buf, sizeof(buf), MSG_DONTWAIT);
        if (ADMIN_BANNING == buf.opt)
        {
            //若接收到被禁言的消息，则聊天权限标志位置1，不能发言
            client.disabledchat = 1;
        }
        else if (ADMIN_DIS_BANNING == buf.opt)
        {
            //若接收到被解除禁言的消息，则聊天权限标志位置0，可以正常发言
            client.disabledchat = 0;
        }
        else if (ADMIN_EXITUSR == buf.opt || 0 != client.client_shutdown)
        {
            //被踢出聊天室，跳出循环退出程序
            break;
        }
        // recv v操作
        sem_post(&(sem_recv));
        usleep(500000);
    }
    system("clear");
    printf("已被踢出聊天室\n");
    Exit();
}

int Login()
{
    int ret;

    char tmp[128] = {0};

    struct info buf;

    //登录操作
    buf.opt = U_LOGIN;
    printf("账号：");
    //若输入的账号格式不正确，会提示重新输入，直到格式正确为止
    while (1)
    {
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);
        if (6 == strlen(tmp))
        {
            break;
        }
        printf("账号为6位数字，请重新输入！\n");
        printf("账号：");
    }
    strncpy(buf.usr.id, tmp, 6);
    printf("密码：");
    //当输入的密码格式不正确时，会提示重新输入，直到格式正确为止
    while (1)
    {
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);

        int passwd_len = strlen(tmp);
        if (passwd_len <= 13 && passwd_len >= 6)
        {
            break;
        }
        printf("密码为6-13位任意字符，请重新输入！\n");
        printf("密码：");
    }
    strncpy(buf.usr.passwd, tmp, 13);
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");

        return FAILURE;
    }
    //清空缓冲区buf，用于接收消息
    memset(&buf, 0, sizeof(buf));
    ret = recv(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");

        return FAILURE;
    }
    // buf.text[0] = 'f'则说明登录失败，账号或密码错误
    if (U_LOGIN == buf.opt && 'f' == buf.text[0])
    {
        printf("登录失败，账号或密码错误！\n");

        return FAILURE;
    }
    // buf.text[0] = 'l'则说明登录失败，账号已经登录，处于在线状态
    else if (U_LOGIN == buf.opt && 'l' == buf.text[0])
    {
        printf("账号被登录，处于在线状态！\n");

        return FAILURE;
    }
    // buf.text[0] = 's'则说明登录成功
    //读入账号在服务器端的账号与昵称
    else if (U_LOGIN == buf.opt && 's' == buf.text[0])
    {
        strncpy(client.myinfo.id, buf.usr.id, 6);
        strncpy(client.myinfo.name, buf.usr.name, 15);
        printf("登录成功，欢迎%s\n", client.myinfo.name);
        sleep(1);

        sem_init(&sem_recv, 0, 1);
        //创建监听线程，负责监听是否被禁言或者踢出聊天室
        if (0 != strcmp("admin0", client.myinfo.id))
        {
            ret = pthread_create(&tid[1], NULL, ban_exit_handler, NULL);
            if (0 != ret)
            {
                ERRLOG("pthread_create");
            }
        }

        return SUCCESS;
    }

    return FAILURE;
}

int Register()
{
    int ret, i, confirm;
    //判定账号格式是否正确标志
    int flag = 1;

    char tmp[128] = {0};
    struct info buf;

    //进行注册操作
    buf.opt = U_REGISTER;
    printf("\033[33m***请输入账号，6位数字***\n");
    printf("\033[37m账号：");
    //若输入的账号格式不正确，会提示重新输入，直到格式正确为止
    while (1)
    {
        //判定账号格式是否正确标志置1
        flag = 1;
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);
        for (i = 0; i < 6; i++)
        {
            //若输入的字符不在'0'和'9'之间，则格式错误，flag置0
            if (tmp[i] < '0' || tmp[i] > '9')
            {
                flag = 0;
            }
        }
        //若账号字符都是'0'和'9'之间，且长度为6，则格式正确，跳出循环
        if ((6 == strlen(tmp)) && (1 == flag))
        {
            break;
        }
        printf("账号为6位数字，请重新输入！\n");
        printf("账号：");
    }
    strncpy(buf.usr.id, tmp, 6);
    printf("\033[33m***请输入密码，6-13个任意字符***\n");
    printf("\033[37m密码：");
    //当输入的密码格式不正确时，会提示重新输入，直到格式正确为止
    while (1)
    {
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);

        int passwd_len = strlen(tmp);
        if (passwd_len <= 13 && passwd_len >= 6)
        {
            break;
        }
        printf("密码为6-13位任意字符，请重新输入！\n");
        printf("密码：");
    }
    strncpy(buf.usr.passwd, tmp, 13);
    //昵称最长为15个字节
    printf("\033[33m***请输入昵称，最长15个字符***\n");
    printf("\033[37m昵称：");
    while (1)
    {
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);

        int name_len = strlen(tmp);
        if (name_len <= 15)
        {
            break;
        }
        printf("昵称最长为15个字符，请重新输入！\n");
        printf("昵称：");
    }
    strncpy(buf.usr.name, tmp, 15);
    //密保，用于找回密码时使用
    printf("\033[33m***请输入密保，用于找回密码***\n");
    printf("\033[33m***您父亲的名字？***\n");
    printf("\033[37m请输入密保：");
    scanf("%s", buf.text);
    printf("\033[33m\n\n***确定信息***\n");
    printf("\033[37m账号：%s\n", buf.usr.id);
    printf("密码：%s\n", buf.usr.passwd);
    printf("昵称：%s\n", buf.usr.name);
    printf("密保：%s\n", buf.text);
    printf("确定注册？\n\n");
    printf("确定：1       取消：2\n");

    while (1)
    {
        printf("请输入：");
        scanf("%d", &confirm);
        if (2 == confirm)
        {
            //取消注册账号，直接返回
            printf("已取消注册\n");

            return FAILURE;
        }
        else if (1 == confirm)
        {
            //确定修改昵称，跳出循环，执行下面的操作
            break;
        }
        else
        {
            //输入有误，重新输入
            printf("输入有误，请重新输入\n");
            getchar();
        }
    }
    //用户信息结构体为要注册的账号、密码、昵称信息，消息正文为密保
    //发送
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");

        return FAILURE;
    }
    //接收服务器发送的结果
    ret = recv(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");

        return FAILURE;
    }
    //注册失败，账号已存在
    if (U_REGISTER == buf.opt && 'f' == buf.text[0])
    {
        printf("账号已存在\n");

        return FAILURE;
    }
    //注册成功
    else if (U_REGISTER == buf.opt && 's' == buf.text[0])
    {
        return SUCCESS;
    }

    return FAILURE;
}

void Menu()
{
    //设置窗口大小为32行100列
    system("resize -s 32 100");
    system("clear");
    printf("\033[3B\t\t\t\t --------------------------\n");
    printf("\t\t\t\t|        网络聊天室        |\n");
    printf("\t\t\t\t|                          |\n");
    printf("\t\t\t\t|        1.聊天大厅        |\n");
    printf("\t\t\t\t|        2.查看在线用户    |\n");
    printf("\t\t\t\t|        3.查看聊天记录    |\n");
    printf("\t\t\t\t|        4.上传文件        |\n");
    printf("\t\t\t\t|        5.下载文件        |\n");
    printf("\t\t\t\t|        6.修改昵称        |\n");
    printf("\t\t\t\t|        7.修改密码        |\n");
    printf("\t\t\t\t|        8.禁言踢人(管理员)|\n");
    printf("\t\t\t\t|        9.退出程序        |\n");
    printf("\t\t\t\t|                          |\n");
    printf("\t\t\t\t --------------------------\n");
}

void ChatHall()
{
    // recv p操作
    sem_wait(&sem_recv);

    int ret;

    //客户端接收发送消息的缓冲区
    struct info buf;

    char tmp[BUF_MAXSIZE] = {0};
    //获取当前时间
    char *str_t = NULL;

    system("clear");
    printf("\n\n\n\n\t\t\t\t           聊天大厅         \n");
    sleep(1);
    //打开记录聊天记录的文件，开始写入，每个用户有单独的目录
    sprintf(tmp, "mkdir %s", client.myinfo.id);
    system(tmp);
    sprintf(tmp, "mkdir %s/mychatrecords", client.myinfo.id);
    system(tmp);
    sprintf(tmp, "./%s/mychatrecords/%s-records.txt", client.myinfo.id, client.myinfo.id);
    ChatFp = fopen(tmp, "a+");
    if (NULL == ChatFp)
    {
        ERRLOG("fopen");
    }
    //获取当前时间
    str_t = GetTime();
    fprintf(ChatFp, "%s进入聊天大厅\n", str_t);
    //释放空间
    free(str_t);
    str_t = NULL;
    system("clear");

    //用户向服务器发送要进入聊天大厅的信息
    buf.opt = U_CHATHALL;
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //创建线程专门用于接受其他客户端的消息
    ret = pthread_create(&tid[0], NULL, RecvFromHall, NULL);

    //发送消息
    while (1)
    {
        printf("\033[37m\033[32;60H1.退出 2.私聊 3.常用话语 4.表情");
        printf("\033[37m\033[32;0H");
        scanf("%s", buf.text);
        //清除在终端显示的刚刚发送的内容
        printf("\033[37m\033[31;0H");
        printf("\033[K");
        fflush(stdout);
        if (cur >= 1)
        {
            cur--;
        }
        //输入'1‘ --- 直接退出聊天室
        if (1 == strlen(buf.text) && '1' == buf.text[0])
        {
            QuitChat();
            break;
        }
        //若被禁言，仅打印出被禁言消息
        if (0 != client.disabledchat)
        {
            printf("\033[29;60H\033[K");
            printf("\033[30;60H\033[33m您已被禁言");
            printf("\033[37m\n");
        }
        else
        {

            //若选择了聊天室内2-4号子功能，则进行相应操作
            if ((1 == strlen(buf.text)) && (buf.text[0] >= '2' && buf.text[0] <= '4'))
            {
                //私聊其他用户（仅目标用户可见）
                if ('2' == buf.text[0])
                {
                    PrivateChat();
                }
                //快捷发送常用话语
                else if ('3' == buf.text[0])
                {
                    SendComm();
                }
                //快捷发送表情
                else if ('4' == buf.text[0])
                {
                    SendEmoji();
                }
            }
            //正常将消息发送到聊天大厅，所有人可见
            else
            {
                buf.opt = U_CHATHALL;
                strncpy(buf.usr.id, client.myinfo.id, 6);
                strncpy(buf.usr.name, client.myinfo.name, 15);
                //将发送消息发送到服务器
                ret = send(client.sockfd, &buf, sizeof(buf), 0);
                if (-1 == ret)
                {
                    ERRLOG("send");
                }
                //若输入消息为'1'，则退出聊天大厅
                //将自己发送的信息打印在聊天大厅当前的位置
                printf("\033[32m\033[%d;0H%s（我）:%s\n", cur, client.myinfo.name, buf.text);
                //获取当前时间
                str_t = GetTime();
                //保存聊天记录
                fprintf(ChatFp, "%s%s（我）：%s\n", str_t, client.myinfo.name, buf.text);
                //释放空间
                free(str_t);
                str_t = NULL;
                if (cur <= 31)
                {
                    cur++;
                }
            }
        }
    }
    // recv v操作
    sem_post(&sem_recv);
    pthread_cancel(tid[0]);
    pthread_join(tid[0], NULL);
    tid[0] = 0;

    return;
}

void *RecvFromHall(void *arg)
{

    //线程函数，用于接收其他在聊天大厅的客户端发的消息
    int ret;

    struct info buf;

    //获取当前时间
    char *str_t;

    while (1)
    {
        //接收消息
        memset(&buf, 0, sizeof(buf));
        ret = recv(client.sockfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("recv");
        }
        if (ADMIN_BANNING == buf.opt)
        {
            //接收到的为被禁言的消息，则聊天权限标志位client.disabledchat置1，不能发言
            client.disabledchat = 1;
        }
        else if (ADMIN_DIS_BANNING == buf.opt)
        {
            //接收到的为被解除禁言的消息，则聊天权限标志位client.disabledchat置0，可以正常发言
            client.disabledchat = 0;
        }
        else if (ADMIN_EXITUSR == buf.opt)
        {
            //线程执行函数接收到被踢出聊天室的消息，客户端状态置1，并设置自身为分离属性，跳出循环，停止程序运行
            client.client_shutdown = 1;
            pthread_detach(pthread_self());
            break;
        }
        else if (U_CHATHALL == buf.opt)
        {
            //将接收到的信息打印在聊天大厅当前的位置
            // option=3，默认消息，所有人可见
            printf("\033[37m\033[%d;0H%s %s:%s\n", cur, buf.usr.id, buf.usr.name, buf.text);
            //保存聊天记录
            str_t = GetTime();
            fprintf(ChatFp, "%s%s %s:%s\n", str_t, buf.usr.id, buf.usr.name, buf.text);
            free(str_t);
            str_t = NULL;
        }
        // option=5，消息为私聊消息，只有自己可见
        else if (U_PRIVATE_CHAT == buf.opt)
        {
            printf("\033[33m\033[%d;0H%s %s（私聊）:%s\n", cur, buf.usr.id, buf.usr.name, buf.text);
            //保存聊天记录
            str_t = GetTime();
            fprintf(ChatFp, "%s%s %s（私聊）:%s\n", str_t, buf.usr.id, buf.usr.name, buf.text);
            free(str_t);
            str_t = NULL;
        }
        printf("\033[37m\033[31;60H\033[K");
        printf("\033[37m\033[32;60H1.退出 2.私聊 3.常用话语 4.表情");
        if (cur <= 31)
        {
            cur++;
        }
        // 将光标位置重定位到底部用于输入要发送的内容
        printf("\033[37m\033[32;0H");
        fflush(stdout);
    }
    system("clear");
    printf("已被踢出聊天室\n");
    Exit();
}

void QuitChat()
{
    int ret;

    //客户端接收发送消息的缓冲区
    struct info buf;

    //获取当前时间
    char *str_t = NULL;

    str_t = GetTime();
    fprintf(ChatFp, "%s退出聊天大厅\n", str_t);
    //释放空间
    free(str_t);
    str_t = NULL;
    //关闭记录聊天记录文件流
    fclose(ChatFp);
    ChatFp = NULL;
    //重置聊天大厅内容打印光标位置记录
    cur = 12;
    //将要退出聊天大厅的消息告知服务器
    // option=4，用户信息结构体无内容，消息正文无内容
    memset(&buf, 0, sizeof(buf));
    buf.opt = U_QUITCHAT;
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    return;
}

void PrivateChat()
{
    int ret;

    //客户端接收发送消息的缓冲区
    struct info buf;

    //获取当前时间
    char *str_t = NULL;

    // option=5，代表这是私聊操作
    memset(&buf, 0, sizeof(buf));
    buf.opt = U_PRIVATE_CHAT;
    //将要打印的信息通过printf函数打印在特定位置
    printf("\033[33m\033[31;60H请输入私聊信息（格式：用户ID-消息） ");
    printf("\033[37m\033[32;60H1.退出 2.私聊 3.常用话语 4.表情");
    printf("\033[37m\033[32;0H");
    scanf("%s", buf.text);
    //清除在终端显示的刚刚发送的内容
    printf("\033[37m\033[31;0H");
    printf("\033[K");
    fflush(stdout);
    //发送消息
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    //清除提示消息
    printf("\033[30;60H\033[K");
    //将自己发送的信息打印在聊天大厅当前的位置
    printf("\033[33m\033[%d;0H%s（我）私聊 %s\n", cur, client.myinfo.name, buf.text);
    //保存聊天记录
    str_t = GetTime();
    fprintf(ChatFp, "%s%s（我）私聊 %s\n", str_t, client.myinfo.name, buf.text);
    free(str_t);
    str_t = NULL;
    if (cur <= 31)
    {
        cur++;
    }
}

void SendComm()
{
    int choice, ret;

    char *comm = NULL;
    //发送消息缓冲区
    struct info buf;

    //将要打印的信息通过printf函数打印在特定位置
    printf("\033[33m\033[31;60H请选择您要发送的话语");
    printf("\033[37m\033[32;60H1.退出 2.私聊 3.常用话语 4.表情");
    while (1)
    {
        //选择快捷常用话语前，打印常用话语表
        CommonWords(0);
        printf("\033[37m\033[32;0H");
        // choice=1-8
        scanf("%d", &choice);
        //清除在终端显示的刚刚输入的内容
        printf("\033[37m\033[31;0H");
        printf("\033[K");
        fflush(stdout);
        //接收选择常用话语所在的内存空间
        comm = CommonWords(choice);
        if (NULL != comm)
        {
            break;
        }
        getchar();
        printf("\033[33m\033[31;60H输入有误！请重新输入");
    }
    memset(&buf, 0, sizeof(buf));
    // 正常将消息发送到聊天大厅，所有人可见
    // 发送的消息为
    buf.opt = U_CHATHALL;
    strncpy(buf.usr.id, client.myinfo.id, 6);
    strncpy(buf.usr.name, client.myinfo.name, 15);
    strncpy(buf.text, comm, BUF_MAXSIZE);
    //释放存放常用话语的空间
    free(comm);
    comm = NULL;
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    //将自己发送的信息打印在聊天大厅当前的位置
    printf("\033[32m\033[%d;0H%s（我）:%s\n", cur, client.myinfo.name, buf.text);
    if (cur <= 31)
    {
        cur++;
    }
}

char *CommonWords(const int choice)
{
    /**
     * @brief
     * 常用话语表
     * 当参数为常用话语表下标时
     * 返回值为相应的常用话语
     *
     * 当参数不在常用话语表下标内时
     * 打印常用话语表
     * 返回值为空
     */
    char words[8][BUF_MAXSIZE] = {
        "今天天气很好，东教一篮球场见！",
        "今天天气很差，看来打不了球了，明天吧！",
        "明天就是星期五了，又可以打球了！",
        "今天是个好日子，开卷开卷！",
        "帮我带份饭，两荤一素！",
        "上号上号！",
        "帮我带瓶农夫山泉，大瓶的！",
        "今天状态不佳！！！"};

    int i;
    int index = 23;

    //清除上次在终端打印的常用话语表
    for (i = 0; i < 9; i++)
    {
        printf("\033[%d;60H\033[K", index - 1);
        index++;
    }
    //参数不在常用话语表下标内时，打印常用话语表，返回值为空
    if (choice - 1 < 0 || choice - 1 > 7)
    {
        index = 23;
        for (i = 0; i < 8; i++)
        {
            printf("\033[33m\033[%d;60H%d.%s", index, i + 1, words[i]);
            index++;
        }

        return NULL;
    }
    //当参数为常用话语表下标时,返回值为相应的常用话语
    char *tmp = (char *)malloc(sizeof(char) * BUF_MAXSIZE);

    strncpy(tmp, words[choice - 1], BUF_MAXSIZE);

    return tmp;
}

void SendEmoji()
{
    int choice, ret;

    char *comm = NULL;
    //发送消息缓冲区
    struct info buf;

    //将要打印的信息通过printf函数打印在特定位置
    printf("\033[33m\033[31;60H请选择您要发送的表情");
    printf("\033[37m\033[32;60H1.退出 2.私聊 3.常用话语 4.表情");
    while (1)
    {
        //选择快捷表情前，打印表情表
        Emojis(0);
        printf("\033[37m\033[32;0H");
        // choice=1-8
        scanf("%d", &choice);
        //清除在终端显示的刚刚输入的内容
        printf("\033[37m\033[31;0H");
        printf("\033[K");
        fflush(stdout);
        //接收选择表情所在的内存空间
        comm = Emojis(choice);
        if (NULL != comm)
        {
            break;
        }
        getchar();
        printf("\033[33m\033[31;60H输入有误！请重新输入");
    }
    memset(&buf, 0, sizeof(buf));
    // 正常将消息发送到聊天大厅，所有人可见
    // 发送的消息为
    buf.opt = U_CHATHALL;
    strncpy(buf.usr.id, client.myinfo.id, 6);
    strncpy(buf.usr.name, client.myinfo.name, 15);
    strncpy(buf.text, comm, BUF_MAXSIZE);
    //释放存放表情的空间
    free(comm);
    comm = NULL;
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    //将自己发送的信息打印在聊天大厅当前的位置
    printf("\033[32m\033[%d;0H%s（我）:%s\n", cur, client.myinfo.name, buf.text);
    if (cur <= 31)
    {
        cur++;
    }
}

char *Emojis(const int choice)
{
    /**
     * @brief
     * 表情表
     * 当参数为表情表下标时
     * 返回值为相应的常用话语
     *
     * 当参数不在表情表下标内时
     * 打印表情表
     * 返回值为空
     */
    char emojis[8][BUF_MAXSIZE] = {
        "^-^",
        "O 。o ~!!",
        "( *￣▽￣)",
        "╥﹏╥...",
        "（≧∀≦）",
        "╮（╯＿╰）╭",
        "ฅ'ω'ฅ",
        "( ¬､¬)"};

    int i;
    int index = 23;

    //清除上次在终端打印的表情表
    for (i = 0; i < 9; i++)
    {
        printf("\033[%d;60H\033[K", index - 1);
        index++;
    }
    //参数不在表情表下标内时，打印表情表，返回值为空
    if (choice - 1 < 0 || choice - 1 > 7)
    {
        index = 23;
        for (i = 0; i < 8; i++)
        {
            printf("\033[33m\033[%d;60H%d.%s", index, i + 1, emojis[i]);
            index++;
        }

        return NULL;
    }
    //当参数为表情表下标时,返回值为相应的表情
    char *tmp = (char *)malloc(sizeof(char) * BUF_MAXSIZE);

    strncpy(tmp, emojis[choice - 1], BUF_MAXSIZE);

    return tmp;
}

void ViewOnlineusers()
{
    // recv p操作
    sem_wait(&sem_recv);

    int ret;
    //发送接收消息缓冲区
    struct info buf;

    memset(&buf, 0, sizeof(buf));
    // option=6代表客户端要查询在线用户的信息
    //消息正文无内容
    buf.opt = U_VIEW_ONLINE;
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    //清空缓冲区用于接收消息
    memset(&buf, 0, sizeof(buf));
    ret = recv(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    //接收到在线用户用户表
    if (U_VIEW_ONLINE == buf.opt)
    {
        system("clear");
        printf("\n\n\n\n\t\t\t\t           在线用户         \n");
        printf("\n\n\t\t\t\t    账号            昵称         \n");
        //记录当前指向的消息正文的下标
        int index = 0;

        //依次打印出在线用户信息
        while (0 != strlen(buf.text + index))
        {
            char id[7] = {0};
            char name[16] = {0};
            strncpy(id, buf.text + index, 6);
            index = index + 7;
            strncpy(name, buf.text + index, 15);
            index = index + 16;
            printf("\t\t\t\t    %s          %s\n", id, name);
        }
    }

    // recv v操作
    sem_post(&sem_recv);

    //按下回车键时返回主页
    printf("\033[31;85H按回车键返回\n");
    while (1)
    {
        if (getchar())
        {
            break;
        }
        sleep(2);
    }
}

void ViewChattingRecords()
{
    char tmp[BUF_MAXSIZE] = {0};

    system("clear");
    //定位聊天记录文件所在路径
    sprintf(tmp, "./%s/mychatrecords/%s-records.txt", client.myinfo.id, client.myinfo.id);
    ChatFp = fopen(tmp, "r");
    if (NULL == ChatFp)
    {
        return;
    }
    //清空tmp内容
    memset(tmp, 0, sizeof(tmp));
    //读聊天记录，读一行，打印一行%*c把'\n'吞掉
    while (EOF != fscanf(ChatFp, "%[^\n]%*c", tmp))
    {
        printf("%s\n", tmp);
        memset(tmp, 0, sizeof(tmp));
    }
    fclose(ChatFp);
    ChatFp = NULL;
    //按下回车键时返回主页
    printf("\033[31;85H按回车键返回\n");
    while (1)
    {
        if (getchar())
        {
            break;
        }
        sleep(2);
    }

    return;
}

void UploadFiles()
{
    // recv p操作
    sem_wait(&sem_recv);

    int ret, tmp;
    //发送文件的大小
    unsigned int filesize = 0;

    //发送文件缓冲区
    struct info buf;

    char upload_result = 'x';
    //文件名
    char filename[128] = {0};

    //打开的文件指针
    FILE *send_fp = NULL;

    system("clear");
    printf("请输入文件名：");
    scanf("%s", filename);
    printf("%s\n", filename);
    //以只读方式打开文件，若fopen后文件指针仍为空，则说明文件不存在，直接返回
    send_fp = fopen(filename, "r");
    if (NULL == send_fp)
    {
        printf("文件打开失败，文件不存在!\n两秒后返回主页面\n");
        sleep(2);

        return;
    }
    //先把文件名上传，服务器创建相应的文件
    memset(&buf, 0, sizeof(buf));
    buf.opt = U_UPLOAD_FILE;
    strncpy(buf.text, filename, 128);
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //向服务器发送要发送的文件的大小
    fseek(send_fp, 0, SEEK_END);
    filesize = ftell(send_fp);
    fseek(send_fp, 0, SEEK_SET);

    ret = send(client.sockfd, &filesize, sizeof(filesize), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    printf("filesize is %d\n", filesize);

    //开始发送文件
    while (1)
    {
        char sendfile_buf[1024] = {0};

        //当剩余文件长度为0时，则传输结束，跳出循环
        if (0 == filesize)
        {
            printf("文件传输结束\n");
            break;
        }
        //记录读入缓冲区的字节数
        tmp = fread(sendfile_buf, 1, sizeof(sendfile_buf), send_fp);
        //记录实际发送的字节数
        tmp = send(client.sockfd, sendfile_buf, tmp, 0);
        if (-1 == tmp)
        {
            ERRLOG("send");
        }
        //更新剩余字节数
        filesize -= tmp;
    }

    //等待接收服务器接收文件成功的回应
    ret = recv(client.sockfd, &upload_result, sizeof(upload_result), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    if ('s' == upload_result)
    {
        printf("文件下载成功\n");
    }

    // recv v操作
    sem_post(&sem_recv);

    //按下回车键时返回主页
    printf("\033[31;85H按回车键返回\n");
    while (1)
    {
        if (getchar())
        {
            break;
        }
        sleep(2);
    }
    fclose(send_fp);
}

void DownloadFiles()
{
    // recv p操作
    sem_wait(&sem_recv);

    int ret, tmp;
    //接收文件剩余字节数
    unsigned int filesize = 0;

    //接收文件缓冲区
    struct info buf;

    char isexist;
    char recvresult;
    //文件名
    char filename[128] = {0};
    char tmp_recv_file[256] = {0};

    //作为下载文件的文件指针
    FILE *fp = NULL;

    memset(&buf, 0, sizeof(buf));
    // option=8，下载客户端文件
    buf.opt = U_DOWN_FILE;

    //向服务器请求要下载文件
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");

        return;
    }

    system("clear");
    printf("可下载文件\n");
    //服务器返回在可下载文件的文件名，打印出文件名

    //接收存放可下载文件名的文件的大小
    ret = recv(client.sockfd, &filesize, sizeof(filesize), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }

    while (1)
    {
        char recvfile_buf[1024] = {0};

        if (0 == filesize)
        {
            break;
        }
        /**
         * @brief
         * 若是剩余字节数大于接收缓冲区大小
         * 则一次接收缓冲区大小的字节数
         * 否则仅接收剩余字节数
         */
        if (filesize > sizeof(recvfile_buf))
        {
            tmp = recv(client.sockfd, recvfile_buf, sizeof(recvfile_buf), 0);
        }
        else
        {
            tmp = recv(client.sockfd, recvfile_buf, filesize, 0);
        }
        if (-1 == tmp)
        {
            ERRLOG("recv");
        }
        //直接打印出接收内容（可下载文件的文件名）
        printf("%s", recvfile_buf);
        //更新剩余字节数
        filesize -= tmp;
    }
    //输入要下载文件的文件名
    printf("请输入待下载文件的文件名：");
    scanf("%s", filename);

    //向服务器发送要下载文件的文件名
    ret = send(client.sockfd, filename, sizeof(filename), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //接收服务器发送的文件是否存在的结果
    ret = recv(client.sockfd, &isexist, sizeof(isexist), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    //若接收到的消息为'n'，则说明文件不存在，直接返回
    if ('n' == isexist)
    {
        printf("文件不存在...\n两秒后返回主页面\n");
        sleep(2);
    }
    //服务器通知文件存在，开始下载
    else if ('y' == isexist)
    {
        //接收要下载文件的大小
        ret = recv(client.sockfd, &filesize, sizeof(filesize), 0);
        if (-1 == ret)
        {
            ERRLOG("recv");
        }
        // printf("filesize is %d\n", filesize);
        printf("文件存在，开始下载...\n");

        printf("%s\n", client.myinfo.id);
        //创建存储个人文件的专属目录（若不存在则创建）
        memset(tmp_recv_file, 0, sizeof(tmp_recv_file));
        sprintf(tmp_recv_file, "mkdir %s", client.myinfo.id);
        system(tmp_recv_file);
        //创建存储下载文件的目录（若不存在则创建）
        memset(tmp_recv_file, 0, sizeof(tmp_recv_file));
        sprintf(tmp_recv_file, "mkdir ./%s/mydownloadfiles", client.myinfo.id);
        system(tmp_recv_file);

        //开始下载文件
        memset(tmp_recv_file, 0, sizeof(tmp_recv_file));
        sprintf(tmp_recv_file, "./%s/mydownloadfiles/%s", client.myinfo.id, filename);
        fp = fopen(tmp_recv_file, "w");
        if (NULL == fp)
        {
            ERRLOG("fopen");
        }

        //开始接受文件
        while (1)
        {
            char recvfile_buf[1024] = {0};

            //剩余字节数为0，说明文件接收完毕，退出循环
            if (0 == filesize)
            {
                printf("文件传输结束\n");
                break;
            }
            /**
             * @brief
             * 若是剩余字节数大于接收缓冲区大小
             * 则一次接收缓冲区大小的字节数
             * 否则仅接收剩余字节数
             */
            if (filesize > sizeof(recvfile_buf))
            {
                tmp = recv(client.sockfd, recvfile_buf, sizeof(recvfile_buf), 0);
            }
            else
            {
                tmp = recv(client.sockfd, recvfile_buf, filesize, 0);
            }
            if (-1 == tmp)
            {
                ERRLOG("recv");
            }
            fwrite(recvfile_buf, tmp, 1, fp);
            //更新剩余字节数
            filesize -= tmp;
            // printf("tmp is %d\n", tmp);
            // printf("filesize is %d\n", filesize);
        }
        //接收服务器的确认消息
        ret = recv(client.sockfd, &recvresult, sizeof(recvresult), 0);
        if (-1 == ret)
        {
            ERRLOG("recv");
        }
        if ('s' == recvresult)
        {
            printf("文件下载成功...\n");
        }
        printf("\033[31;85H按回车键返回\n");
        while (1)
        {
            if (getchar())
            {
                break;
            }
            sleep(2);
        }

        fclose(fp);
    }
    // recv v操作
    sem_post(&sem_recv);

    return;
}

void UpdateName()
{
    // recv p操作
    sem_wait(&(sem_recv));

    int ret;
    //修改昵称时确定项
    int confirm;

    //接收呢称是否修改成功的结果
    char update_result;
    //新的昵称
    char newname[16] = {0};

    //向服务器发送的消息
    struct info buf;

    //输入新的昵称
    system("clear");
    printf("\033[33m***修改昵称***\n");
    printf("\033[37m现昵称为：%s\n\n", client.myinfo.name);
    while (1)
    {
        //当输入的昵称格式正确时才跳出循环
        printf("请输入新的昵称：");
        scanf("%s", newname);
        if (strlen(newname) <= 15)
        {
            break;
        }
        printf("昵称最长为15个字符\n");
    }

    //若与原有昵称一样，则直接返回
    if (0 == strcmp(client.myinfo.name, newname))
    {
        printf("与现昵称相同\n");
        printf("两秒后返回主页面\n");
        sleep(2);

        return;
    }

    //是否确定修改昵称
    printf("\n新的昵称为：%s\n", newname);
    printf("确定修改昵称为：%s？\n\n", newname);
    printf("确定：1       取消：2\n");

    while (1)
    {
        printf("请输入：");
        scanf("%d", &confirm);
        if (2 == confirm)
        {
            //取消修改昵称，直接返回
            printf("两秒后返回主页面\n");
            sleep(2);

            return;
        }
        else if (1 == confirm)
        {
            //确定修改昵称，跳出循环，执行下面的操作
            break;
        }
        else
        {
            //输入有误，重新输入
            printf("输入有误，请重新输入\n");
            getchar();
        }
    }

    //向服务器发送修改昵称的请求
    memset(&buf, 0, sizeof(buf));
    buf.opt = U_UPDATE_NAME;
    //要修改昵称的用户的账号ID
    strncpy(buf.usr.id, client.myinfo.id, 6);
    //要修改昵称的用户的新昵称
    strncpy(buf.usr.name, newname, 15);

    //将消息打包好发送给服务器
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //接收昵称是否修改成功的结果
    ret = recv(client.sockfd, &update_result, sizeof(update_result), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    if ('s' == update_result)
    {
        printf("昵称修改成功\n重新登录后生效\n");
    }
    else if ('f' == update_result)
    {
        printf("昵称修改失败\n");
    }

    // recv v操作
    sem_post(&(sem_recv));

    printf("\033[31;85H按回车键返回\n");
    while (1)
    {
        if (getchar())
        {
            break;
        }
        sleep(2);
    }
}

void RetrievePasswd()
{
    /**
     * @brief
     * ret 用于临时保存函数返回值来判断和是否发生了错误
     * i 用于循环判断账号格式是否正确
     * flag 标志，若账号格式正确则为1，否则置0
     */
    int ret, i, flag;

    //用于临时存放输入的账号
    char tmp[128] = {0};

    struct info buf;

    memset(&buf, 0, sizeof(buf));
    system("clear");
    printf("\033[33m***找回密码***\n");
    printf("\033[37m请输入账号：");
    while (1)
    {
        //判定账号格式是否正确标志置1
        flag = 1;
        scanf("%s", tmp);
        for (i = 0; i < 6; i++)
        {
            //若输入的字符不在'0'和'9'之间，则格式错误，flag置0
            if (tmp[i] < '0' || tmp[i] > '9')
            {
                flag = 0;
            }
        }
        //若账号字符都是'0'和'9'之间，且长度为6，则格式正确，跳出循环
        if ((6 == strlen(tmp)) && (1 == flag))
        {
            break;
        }
        memset(tmp, 0, sizeof(tmp));
        printf("账号为6位数字，请重新输入！\n");
        printf("账号：");
    }
    //输入密保的回答
    printf("您父亲的名字是？\n");
    printf("请输入：");
    scanf("%s", buf.text);
    buf.opt = U_RETRIEVE_PASSWD;
    strncpy(buf.usr.id, tmp, 6);

    //向服务器发送打包好的消息
    //用户信息结构体为账号ID，其他项无内容，消息正文为密保问题的回答
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //接收服务器的回应
    ret = recv(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    //若收到了'f'，则说明密保问题回答错误，否则说明找回密码成功并打印
    if ((U_RETRIEVE_PASSWD == buf.opt) && (1 == strlen(buf.text)) && 'f' == buf.text[0])
    {
        printf("密保错误，找回密码失败\n");
    }
    else
    {
        printf("找回密码成功\n");
        printf("\033[33m密码为：%s\n", buf.usr.passwd);
    }
    getchar();
    printf("\033[37m\033[31;85H按回车键返回\n");
    while (1)
    {
        if (getchar())
        {
            break;
        }
        sleep(2);
    }

    return;
}

void UpdatePasswd()
{
    // recv p操作
    sem_wait(&(sem_recv));

    int ret, confirm;

    //用于接收服务器的结果
    char update_result;
    //临时存放输入的数据
    char tmp[128] = {0};

    //发送接收消息缓冲区
    struct info buf;

    memset(&buf, 0, sizeof(buf));
    system("clear");
    printf("\033[33m***修改密码***\n");
    //输入原密码
    while (1)
    {
        printf("\033[37m请输入原密码：");
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);

        //直到输入密码格式正确时跳出循环
        int length = strlen(tmp);
        if (length >= 6 && length <= 13)
        {
            break;
        }
        printf("密码为6-13个字符，请重新输入\n");
    }
    //将原密码放到用户信息结构体的密码项
    strncpy(buf.usr.passwd, tmp, 13);

    //输入新的密码
    while (1)
    {
        printf("\033[37m请输入新的密码：");
        memset(tmp, 0, sizeof(tmp));
        scanf("%s", tmp);

        //直到输入密码格式正确时跳出循环
        int length = strlen(tmp);
        if (length >= 6 && length <= 13)
        {
            break;
        }
        printf("密码为6-13个字符，请重新输入\n");
    }
    //将新的密码放到消息正文
    strncpy(buf.text, tmp, 13);

    //是否确定修改密码
    printf("\n新密码为：%s\n", buf.text);
    printf("确定修改？\n\n");
    printf("确定：1       取消：2\n");

    while (1)
    {
        printf("请输入：");
        scanf("%d", &confirm);
        if (2 == confirm)
        {
            //取消修改密码，直接返回
            printf("两秒后返回主页面\n");
            sleep(2);

            return;
        }
        else if (1 == confirm)
        {
            //确定修改密码，跳出循环，执行下面的操作
            break;
        }
        else
        {
            //输入有误，重新输入
            printf("输入有误，请重新输入\n");
            getchar();
        }
    }

    //标志修改密码操作
    buf.opt = U_UPDATE_PASSWD;
    strncpy(buf.usr.id, client.myinfo.id, 6);

    //向服务器发送打包好的消息
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //接收服务器发来的修改密码的结果
    ret = recv(client.sockfd, &update_result, sizeof(update_result), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    if ('f' == update_result)
    {
        //修改密码失败
        printf("原密码错误，修改密码失败\n");
    }
    else if ('e' == update_result)
    {
        //修改密码失败
        printf("输入的新密码与原密码相同，修改密码失败\n");
    }
    else if ('s' == update_result)
    {
        //修改密码成功
        printf("修改密码成功\n");
    }

    // recv v操作
    sem_post(&(sem_recv));

    printf("\033[37m\033[31;85H按回车键返回\n");
    while (1)
    {
        if (getchar())
        {
            break;
        }
        sleep(2);
    }
}

void admin()
{
    /**
     * @brief
     * choice 选择的操作
     * i 用作循环
     * flag 标志位，用作判断账号格式是否正确
     */
    int choice, i, flag;

    //临时存放输入的数据
    char tmp[128] = {0};
    //存放要禁言或踢出的账号id
    char managed_id[7] = {0};

    system("clear");
    if (0 != strcmp("admin0", client.myinfo.id))
    {
        printf("\n仅限于管理员操作\n");
        sleep(2);

        return;
    }
    printf("\033[33m\n\n***禁言踢人***\n");
    printf("\033[37m\n1.禁言\t\t2.解除禁言\t\t3.踢人\n");
    //保证选择无误
    while (1)
    {
        printf("请选择：");
        scanf("%d", &choice);
        if (1 == choice || 2 == choice || 3 == choice)
        {
            break;
        }
        printf("输入错误，请重新输入\n");
        getchar();
    }

    if (1 == choice)
    {
        //进行禁言操作
        printf("\n\033[33m请输入要禁言的账号ID\033[37m\n");
        while (1)
        {
            //判定账号格式是否正确标志置1
            flag = 1;
            scanf("%s", tmp);
            for (i = 0; i < 6; i++)
            {
                //若输入的字符不在'0'和'9'之间，则格式错误，flag置0
                if (tmp[i] < '0' || tmp[i] > '9')
                {
                    flag = 0;
                }
            }
            //若账号字符都是'0'和'9'之间，且长度为6，则格式正确，跳出循环
            if ((6 == strlen(tmp)) && (1 == flag))
            {
                break;
            }
            printf("账号为6位数字，请重新输入！\n");
            printf("账号：");
            memset(tmp, 0, sizeof(tmp));
        }
        //将要禁言的账号ID放到managed_id里面
        strncpy(managed_id, tmp, 6);
        BanningSpeaking(managed_id);
        sleep(1);
    }
    else if (2 == choice)
    {
        //进行禁言操作
        printf("\n\033[33m请输入要解除禁言的账号ID\033[37m\n");
        while (1)
        {
            //判定账号格式是否正确标志置1
            flag = 1;
            scanf("%s", tmp);
            for (i = 0; i < 6; i++)
            {
                //若输入的字符不在'0'和'9'之间，则格式错误，flag置0
                if (tmp[i] < '0' || tmp[i] > '9')
                {
                    flag = 0;
                }
            }
            //若账号字符都是'0'和'9'之间，且长度为6，则格式正确，跳出循环
            if ((6 == strlen(tmp)) && (1 == flag))
            {
                break;
            }
            printf("账号为6位数字，请重新输入！\n");
            printf("账号：");
            memset(tmp, 0, sizeof(tmp));
        }
        //将要禁言的账号ID放到managed_id里面
        strncpy(managed_id, tmp, 6);
        DisabledBanningSpeaking(managed_id);
        sleep(1);
    }
    else if (3 == choice)
    {
        //进行踢人操作
        printf("\n\033[33m请输入要踢出聊天室的账号ID\033[37m\n");
        while (1)
        {
            //判定账号格式是否正确标志置1
            flag = 1;
            scanf("%s", tmp);
            for (i = 0; i < 6; i++)
            {
                //若输入的字符不在'0'和'9'之间，则格式错误，flag置0
                if (tmp[i] < '0' || tmp[i] > '9')
                {
                    flag = 0;
                }
            }
            //若账号字符都是'0'和'9'之间，且长度为6，则格式正确，跳出循环
            if ((6 == strlen(tmp)) && (1 == flag))
            {
                break;
            }
            printf("账号为6位数字，请重新输入！\n");
            printf("账号：");
            memset(tmp, 0, sizeof(tmp));
        }
        //将要踢出聊天室的账号ID放到managed_id里面
        strncpy(managed_id, tmp, 6);
        ExitChatroom(managed_id);
        sleep(1);
    }

    return;
}

void BanningSpeaking(const char *id)
{
    // 仅限管理员
    int ret;

    // 禁言的结果
    char banning_result;

    struct info buf;

    // 这是一个禁言消息，包含了被禁言的账号ID
    memset(&buf, 0, sizeof(buf));
    buf.opt = ADMIN_BANNING;
    strncpy(buf.usr.id, id, 6);

    // 向服务器发送打包好的消息
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    // 接收禁言操作是否成功的结果
    ret = recv(client.sockfd, &banning_result, sizeof(banning_result), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    if ('s' == banning_result)
    {
        printf("禁言成功\n");
    }
    else if ('f' == banning_result)
    {
        printf("禁言失败\n目的账号不在线或不存在\n");
    }
}

void DisabledBanningSpeaking(const char *id)
{
    // 仅限管理员
    int ret;

    // 解除禁言的结果
    char disabledbanning_result;

    struct info buf;

    // 打包消息，这是一个解除禁言的消息，包含被解除禁言的账号ID
    memset(&buf, 0, sizeof(buf));
    buf.opt = ADMIN_DIS_BANNING;
    strncpy(buf.usr.id, id, 6);

    // 向服务器发送打包好的消息
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    // 接收解除禁言操作是否成功的结果
    ret = recv(client.sockfd, &disabledbanning_result, sizeof(disabledbanning_result), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    if ('s' == disabledbanning_result)
    {
        printf("解除禁言成功\n");
    }
    else if ('f' == disabledbanning_result)
    {
        printf("解除禁言失败\n目的账号不在线或不存在\n");
    }
}

void ExitChatroom(const char *id)
{
    // 仅限管理员
    int ret;

    // 踢人的结果
    char exit_result;

    struct info buf;

    // 打包消息，这是一个踢人的消息，包含被踢出聊天室的账号ID
    memset(&buf, 0, sizeof(buf));
    buf.opt = ADMIN_EXITUSR;
    strncpy(buf.usr.id, id, 6);

    // 向服务器发送打包好的消息
    ret = send(client.sockfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    // 接收踢人操作是否成功的结果
    ret = recv(client.sockfd, &exit_result, sizeof(exit_result), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    if ('s' == exit_result)
    {
        printf("已踢出聊天室\n");
    }
    else if ('f' == exit_result)
    {
        printf("踢出聊天室失败\n目的账号不在线或不存在\n");
    }
}

char *GetTime()
{
    char *str_t = (char *)malloc(sizeof(char) * 30);
    if (NULL == str_t)
    {
        return NULL;
    }

    time_t clock;

    //获取时间，保存到time_t结构体中。在time的返回值(sec)里面有全部秒数
    time(&clock);
    strcpy(str_t, ctime(&clock)); //将time_t类型的结构体中的时间，按照一定格式保存成字符串，

    return str_t;
}

void Exit()
{
    int i;

    close(client.sockfd);
    if (NULL != ChatFp)
    {
        fclose(ChatFp);
        ChatFp = NULL;
    }
    // 销毁无名信号灯
    sem_destroy(&sem_recv);

    // 正常退出程序
    exit(0);
}