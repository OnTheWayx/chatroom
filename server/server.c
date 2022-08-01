#include "server.h"

// 服务器结构体，存放服务器基本参数
static struct server_t server;

//创建的线程池
static threadpool *pool = NULL;
//保存服务器启动时即一直工作的两个线程号（接收客户端的连接，转发客户端的消息），回收资源时，直接取消这两个线程
static pthread_t tid[2] = {0};

//数据库句柄
static sqlite3 *ppdb = NULL;
//数据库操作互斥量
static pthread_mutex_t usrsdb_mutex = PTHREAD_MUTEX_INITIALIZER;

//文件指针（用于保存所有人的聊天信息）
static FILE *ChatFp = NULL;

int InitServer()
{
    printf("服务器启动中...\n");
    // 初始化服务器基本参数
    server.sockfd = 0;
    server.epfd = 0;
    server.chat_maxfd = 0;
    server.login_maxfd = 0;
    memset(&(server.events), 0, sizeof(server.events));
    memset(&(server.usrs_chathall), 0, sizeof(server.usrs_chathall));
    memset(&(server.usrs_online), 0, sizeof(server.usrs_online));
    memset(&(server.workingfds), 0, sizeof(server.workingfds));

    //用于临时保存所调用函数的返回值判断是否发生了错误
    int ret, i;

    //若用户信息表不存在，则创建用户信息表
    char sql[128] = {0};

    //数据库操作前上锁
    LOCK(usrsdb_mutex);
    ret = sqlite3_open("./usrs/usrs.db", &ppdb);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_open");

        return FAILURE;
    }
    sprintf(sql, "create table if not exists usr(id char primary key, passwd char, name char);");
    ret = sqlite3_exec(ppdb, sql, NULL, NULL, NULL);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_exec");

        return FAILURE;
    }
    sqlite3_close(ppdb);
    ppdb = NULL;
    //数据库操作后解锁
    UNLOCK(usrsdb_mutex);

    /**
     * 基于ipv4，传输协议为TCP，无其他附加协议
     */
    server.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server.sockfd)
    {
        ERRLOG("socket");

        return FAILURE;
    }

    //设置地址可以被重复绑定
    int opt = 1;
    setsockopt(server.sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //填充网络信息结构体，保存服务器信息
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);

    memset(&server_addr, 0, server_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8888);

    //绑定套接字与网络信息结构体
    ret = bind(server.sockfd, (struct sockaddr *)&server_addr, server_len);
    if (-1 == ret)
    {
        ERRLOG("bind");

        return FAILURE;
    }

    //设置监听队列(套接字，最大等待处理的连接队列的长度)
    ret = listen(server.sockfd, 10);
    if (-1 == ret)
    {
        ERRLOG("bind");

        return FAILURE;
    }

    //中间变量
    struct epoll_event ev;

    //创建epoll对象
    server.epfd = epoll_create(CLIENT_MAXSIZE);
    //创建失败时返回前关闭套接字
    if (-1 == server.epfd)
    {
        ERRLOG("epoll_create");

        return FAILURE;
    }
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = server.sockfd;
    //当server.sockfd可读的时候
    ev.events = EPOLLIN;
    //将server.sockfd（套接字）添加到server.epfd（创建的epoll对象）中
    ret = epoll_ctl(server.epfd, EPOLL_CTL_ADD, server.sockfd, &ev);
    //创建失败时返回前关闭套接字和创建的epoll对象
    if (-1 == ret)
    {
        ERRLOG("epoll_ctl");

        return FAILURE;
    }

    //创建线程池
    //线程数量为10，任务队列最大长度为20
    pool = threadpool_create(10, 20);
    if (NULL == pool)
    {
        ERRLOG("threadpool_create");

        return FAILURE;
    }
    //将监听客户端连接请求的任务添加到任务队列
    thread_add_task(pool, AcceptClient, 0, NULL);
    thread_add_task(pool, TransMsg, 0, NULL);

    printf("服务器运行中...\n");
    //防止main函数结束导致主线程结束
    while (1)
    {
        sleep(50);
    }
}

void CloseServer()
{
    struct epoll_event ev;

    int i;

    //把套接字从epoll对象中删除
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = server.sockfd;
    ev.events = EPOLLIN;
    epoll_ctl(server.epfd, EPOLL_CTL_DEL, server.sockfd, &ev);
    //关闭套接字与epoll对象
    close(server.sockfd);
    close(server.epfd);
    //关闭保存聊天信息的文件指针
    fclose(ChatFp);
    //取消一直工作的这两个线程(接收客户端的连接，转发客户端的消息）
    for (i = 0; i < 2; i++)
    {
        pthread_cancel(tid[i]);
    }
    threadpool_destroy(pool);
    printf("资源回收完成...\n");
    printf("服务器已关闭...\n");
    //结束主程序
    exit(0);
}

void AcceptClient(const int recvfd, const void *recvinfo)
{
    struct epoll_event ev;

    //客户端网络信息结构体
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int num, i, ret;

    //保存一直工作的第一个线程的线程号(接收客户端的连接）
    tid[0] = pthread_self();
    while (1)
    {
        //阻塞等待发生事件
        num = epoll_wait(server.epfd, server.events, CLIENT_MAXSIZE, -1);
        if (-1 == num)
        {
            ERRLOG("epoll_wait");
            printf("%d\n", errno);
        }

        for (i = 0; i < num; i++)
        {
            //如果存在发生事件的文件描述符为套接字，则说明有客户端请求连接
            if (server.events[i].data.fd == server.sockfd)
            {
                printf("有客户端请求连接...\n");

                //接收客户端的连接
                int fd = accept(server.sockfd, (struct sockaddr *)&client_addr, &client_len);
                if (-1 == fd)
                {
                    ERRLOG("accept");
                }
                //把接收到的客户端的文件描述符添加到epoll对象中
                memset(&ev, 0, sizeof(ev));
                ev.data.fd = fd;
                ev.events = EPOLLIN;
                ret = epoll_ctl(server.epfd, EPOLL_CTL_ADD, fd, &ev);
                if (-1 == ret)
                {
                    ERRLOG("epoll_ctl");

                    return;
                }
                printf("接收到客户端%d的连接...\n", fd);
            }
        }
    }
}

void TransMsg(const int recvfd, const void *recvinfo)
{
    int num, i, ret;

    struct info buf;

    struct epoll_event ev;

    //保存一直工作的第二个线程的线程号(转发客户端的消息）
    tid[1] = pthread_self();

    //打开保存聊天信息的文件，写入所有经服务器转发的信息
    // system("mkdir chattingrecords");
    ChatFp = fopen("./chattingrecords/records.txt", "a+");
    if (NULL == ChatFp)
    {
        ERRLOG("fopen");
    }
    fprintf(ChatFp, "\n");

    while (1)
    {
        //阻塞等待发生时间
        num = epoll_wait(server.epfd, server.events, CLIENT_MAXSIZE, -1);
        if (-1 == num)
        {
            ERRLOG("epoll_wait");
        }

        for (i = 0; i < num; i++)
        {
            /**
             * @brief
             * 若有事件发生且文件描述符不为套接字则说明有
             * 已连接客户端发送消息，若此文件描述符不在忙文件描述符
             * 数组中，则说明这是新的任务，添加到任务队列中
             *
             */
            if ((0 == server.workingfds[server.events[i].data.fd]) && (server.events[i].data.fd != server.sockfd) && (server.events[i].events & EPOLLIN))
            {
                memset(&buf, 0, sizeof(buf));
                //接收消息
                ret = recv(server.events[i].data.fd, &buf, sizeof(buf), 0);
                if (-1 == ret)
                {
                    ERRLOG("recv");
                }
                /**
                 * 若recv返回0，则说明有客户端退出
                 * 把其文件描述符从epoll对象中移除
                 * 并回收其文件描述符
                 */
                else if (0 == ret)
                {
                    //此文件描述符即将进行传送文件，需要多次recv，加入忙文件描述符server.workingfds数组
                    server.workingfds[server.events[i].data.fd] = server.events[i].data.fd;
                    printf("客户端%d下线...\n", server.events[i].data.fd);
                    thread_add_task(pool, ClearClient, server.events[i].data.fd, NULL);
                }
                else
                {
                    /**
                     * @brief
                     * 处理客户端的请求，识别客户端进行什么操作
                     * 进行相应处理
                     */
                    switch (buf.opt)
                    {
                    case U_LOGIN:
                        //登录
                        thread_add_task(pool, Login, server.events[i].data.fd, &buf);
                        break;
                    case U_REGISTER:
                        //注册
                        thread_add_task(pool, Register, server.events[i].data.fd, &buf);
                        break;
                    case U_CHATHALL:
                        //进入聊天大厅并进行聊天（默认群发）
                        thread_add_task(pool, ChatHall, server.events[i].data.fd, &buf);
                        break;
                    case U_QUITCHAT:
                        //退出聊天大厅
                        thread_add_task(pool, QuitChat, server.events[i].data.fd, NULL);
                        break;
                    case U_PRIVATE_CHAT:
                        thread_add_task(pool, PrivateChat, server.events[i].data.fd, &buf);
                        break;
                    case U_VIEW_ONLINE:
                        //此文件描述符即将进行传送文件，需要多次recv，加入忙文件描述符server.workingfds数组
                        server.workingfds[server.events[i].data.fd] = server.events[i].data.fd;
                        thread_add_task(pool, ViewOnlineusers, server.events[i].data.fd, NULL);
                        break;
                    case U_UPLOAD_FILE:
                        //此文件描述符即将进行传送文件，需要多次recv，加入忙文件描述符server.workingfds数组
                        server.workingfds[server.events[i].data.fd] = server.events[i].data.fd;
                        thread_add_task(pool, RecieveUploadFiles, server.events[i].data.fd, &buf);
                        break;
                    case U_DOWN_FILE:
                        //此文件描述符即将进行传送文件，需要多次recv，加入忙文件描述符server.workingfds数组
                        server.workingfds[server.events[i].data.fd] = server.events[i].data.fd;
                        thread_add_task(pool, SendUploadFiles, server.events[i].data.fd, &buf);
                        break;
                    case U_UPDATE_NAME:
                        //此文件描述符即将进行传送文件，需要多次recv，加入忙文件描述符server.workingfds数组
                        server.workingfds[server.events[i].data.fd] = server.events[i].data.fd;
                        thread_add_task(pool, UpdateName, server.events[i].data.fd, &buf);
                        break;
                    case U_RETRIEVE_PASSWD:
                        thread_add_task(pool, RetrievePasswd, server.events[i].data.fd, &buf);
                        break;
                    case U_UPDATE_PASSWD:
                        //此文件描述符即将进行传送文件，需要多次recv，加入忙文件描述符server.workingfds数组
                        server.workingfds[server.events[i].data.fd] = server.events[i].data.fd;
                        thread_add_task(pool, UpdatePasswd, server.events[i].data.fd, &buf);
                        break;
                    case ADMIN_BANNING:
                        thread_add_task(pool, Admin, server.events[i].data.fd, &buf);
                        break;
                    case ADMIN_DIS_BANNING:
                        thread_add_task(pool, Admin, server.events[i].data.fd, &buf);
                        break;
                    case ADMIN_EXITUSR:
                        thread_add_task(pool, Admin, server.events[i].data.fd, &buf);
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        //每轮询完一遍发生的事件数组，睡眠0.05秒
        usleep(50000);
    }
}

void ClearClient(const int recvfd, const void *recvinfo)
{
    struct epoll_event ev;

    int ret;

    //清空客户端相关的在线信息
    memset(server.usrs_online[recvfd].id, 0, 6);
    memset(server.usrs_online[recvfd].name, 0, 15);
    server.usrs_chathall[recvfd] = 0;
    //将客户端的文件描述符从、epoll对象中移除
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = recvfd;
    ev.events = EPOLLIN;
    ret = epoll_ctl(server.epfd, EPOLL_CTL_DEL, recvfd, &ev);
    if (-1 == ret)
    {
        ERRLOG("epoll_ctl");
    }
    close(recvfd);
    //将此文件描述符从忙文件描述符数组中移除
    server.workingfds[recvfd] = 0;
}

void Login(const int recvfd, const void *recvinfo)
{
    int ret;

    //复制接收到的客户端的数据到buf里来
    struct info buf;

    char sql[128] = {0};
    //存放查询的结果
    char **result;

    int row, column;

    //打开数据库
    //数据库操作前上锁
    LOCK(usrsdb_mutex);
    ret = sqlite3_open("./usrs/usrs.db", &ppdb);
    if (ret != SQLITE_OK)
    {
        ERRLOG("sqlite3_open");
    }
    memcpy(&buf, recvinfo, sizeof(buf));
    //同时查账号和密码，若查到数据，则说明账号密码正确登录成功
    snprintf(sql, 128, "select id,passwd,name from usr where id='%s' and passwd='%s';", buf.usr.id, buf.usr.passwd);
    ret = sqlite3_get_table(ppdb, sql, &result, &row, &column, NULL);
    if (ret != SQLITE_OK)
    {
        ERRLOG("sqlite3_get_table");
    }
    //关闭数据库句柄并置空
    sqlite3_close(ppdb);
    ppdb = NULL;
    //数据库操作后解锁
    UNLOCK(usrsdb_mutex);

    if (0 == row)
    {
        //根据客户端输入的账号密码未查询到数据
        //说明账号不存在或密码错误，返回字符'f'
        memset(&buf, 0, sizeof(buf));
        buf.opt = U_LOGIN;
        buf.text[0] = 'f';
        ret = send(recvfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    else if (1 == row)
    {
        //根据客户端输入的账号密码查询到数据
        //判断账号是否已经被登录
        int i;

        for (i = 0; i <= server.login_maxfd; i++)
        {
            //若账号处于在线状态，则说明账号已经登录
            //返回字符'l'
            if (0 == strcmp(server.usrs_online[i].id, result[3]))
            {
                memset(&buf, 0, sizeof(buf));
                buf.opt = U_LOGIN;
                buf.text[0] = 'l';
                ret = send(recvfd, &buf, sizeof(buf), 0);
                if (-1 == ret)
                {
                    ERRLOG("send");
                }

                return;
            }
        }
        //更新已经登录的用户的最大文件描述符
        if (recvfd > server.login_maxfd)
        {
            server.login_maxfd = recvfd;
        }
        // 根据客户端输入的账号密码查询到了数据
        // 核对信息正确后，客户端登录成功，把客户端的相关信息添加到线上
        strcpy(server.usrs_online[recvfd].id, result[3]);
        strcpy(server.usrs_online[recvfd].name, result[5]);

        memset(&buf, 0, sizeof(buf));
        //将登录成功的消息's'以及有关客户端自身信息返回给客户端
        buf.opt = U_LOGIN;
        buf.text[0] = 's';
        strncpy(buf.usr.id, result[3], 6);
        strncpy(buf.usr.name, result[5], 15);
        ret = send(recvfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    //释放查询的结果
    sqlite3_free_table(result);

    return;
}

void Register(const int recvfd, const void *recvinfo)
{
    int ret;

    //复制recvbuf（客户端发送的数据）到buf里来
    struct info buf;

    //要执行的sql语句，查询该账号是否已存在
    char sql[256] = {0};

    //存放查询的结果
    char **result;
    int row, column;

    //打开数据库
    //数据库操作前上锁
    LOCK(usrsdb_mutex);
    ret = sqlite3_open("./usrs/usrs.db", &ppdb);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_open");
    }
    //把客户端传来的数据复制到buf中
    memcpy(&buf, recvinfo, sizeof(buf));
    //将数据插入数据库，若插入失败，则说明账号（主键）已存在
    snprintf(sql, 256, "insert into usr values('%s','%s','%s','%s');", buf.usr.id, buf.usr.passwd, buf.usr.name, buf.text);
    ret = sqlite3_exec(ppdb, sql, NULL, NULL, NULL);
    sqlite3_close(ppdb);
    ppdb = NULL;
    //数据库操作后解锁
    UNLOCK(usrsdb_mutex);

    if (SQLITE_OK != ret)
    {
        //告知客户端账号已存在，发送一个'f'
        memset(&buf, 0, sizeof(buf));
        buf.opt = U_REGISTER;
        buf.text[0] = 'f';
        ret = send(recvfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    else if (SQLITE_OK == ret)
    {
        //告知客户端账号注册成功，发送一个's'
        memset(&buf, 0, sizeof(buf));
        buf.opt = U_REGISTER;
        buf.text[0] = 's';
        ret = send(recvfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }

    return;
}

void ChatHall(const int recvfd, const void *recvinfo)
{
    int i, ret;

    //复制客户端发送的消息
    struct info buf;

    //用于以字符串形式获取当前时间
    char *str_t = NULL;

    memcpy(&buf, recvinfo, sizeof(buf));
    //记录下最大的文件描述符，减少循环次数
    if (recvfd > server.chat_maxfd)
    {
        server.chat_maxfd = recvfd;
    }
    /*
        当客户端第一次进入聊天大厅时，则更新进入聊天大厅
        的客户端的信息（从服务器记录的登录的客户端的信息中获得）
        默认是群发消息
    */
    if (0 == server.usrs_chathall[recvfd])
    {
        server.usrs_chathall[recvfd] = recvfd;
    }
    //循环次数为记录的聊天大厅内用户的最大文件描述符
    else
    {
        //以字符串形式获取当前时间
        str_t = GetTime();
        fprintf(ChatFp, "%s%s %s:%s\n", str_t, buf.usr.id, buf.usr.name, buf.text);
        //释放空间
        free(str_t);
        //指针释放指向空间后置空
        str_t = NULL;
        for (i = 0; i <= server.chat_maxfd; i++)
        {
            //将消息转发给聊天大厅内的用户（除了自己）
            if (0 != server.usrs_chathall[i] && server.usrs_chathall[i] != recvfd)
            {
                ret = send(server.usrs_chathall[i], &buf, sizeof(buf), 0);
                if (-1 == ret)
                {
                    ERRLOG("send");
                }
            }
        }
    }

    return;
}

void QuitChat(const int recvfd, const void *recvinfo)
{
    //将用户从聊天大厅用户表中移除
    server.usrs_chathall[recvfd] = 0;
    // printf("recvfd=%d退出了聊天大厅\n",recvfd);

    return;
}

void PrivateChat(const int recvfd, const void *recvinfo)
{
    int i, ret;
    int index = 0;

    //以字符串形式获取当前系统时间
    char *str_t;

    // 复制接收到的客户端发送的消息
    // 发送的消息为
    // 私聊账号id 消息内容
    // 0-5 |6| 23-
    struct info buf;
    struct info tmp;

    memcpy(&buf, recvinfo, sizeof(buf));
    //查找目标用户
    for (i = 0; i <= server.chat_maxfd; i++)
    {
        //将消息转发给目标用户
        if (0 == strncmp(server.usrs_online[i].id, buf.text, 6))
        {
            //若目标用户在聊天大厅内，则将消息发送给目标用户
            if (0 != server.usrs_chathall[i])
            {
                str_t = GetTime();
                fprintf(ChatFp, "%s%s %s --> %s\n", str_t, server.usrs_online[recvfd].id, server.usrs_online[recvfd].name, buf.text);
                free(str_t);
                str_t = NULL;
                memset(&tmp, 0, sizeof(tmp));
                // option=5，代表这是私聊消息
                tmp.opt = U_PRIVATE_CHAT;
                strncpy(tmp.usr.id, server.usrs_online[recvfd].id, 6);
                strncpy(tmp.usr.name, server.usrs_online[recvfd].name, 15);
                strncpy(tmp.text, buf.text + 7, sizeof(tmp.text) - 7);
                ret = send(server.usrs_chathall[i], &tmp, sizeof(tmp), 0);
                if (-1 == ret)
                {
                    ERRLOG("send");
                }
            }

            return;
        }
    }

    return;
}

void ViewOnlineusers(const int recvfd, const void *recvinfo)
{
    int i, ret;
    int index = 0;
    //复制客户端发送的消息
    struct info buf;

    memset(&buf, 0, sizeof(buf));
    // option=6代表发送的是在线用户相关信息
    buf.opt = U_VIEW_ONLINE;
    //循环查询在线用户用户表
    for (i = 0; i <= server.login_maxfd; i++)
    {
        if (0 != strlen(server.usrs_online[i].id))
        {
            // 账号 |'\0'| 昵称 |'\0'| 账号 ...
            strncpy(buf.text + index, server.usrs_online[i].id, 6);
            index = index + 7;
            strncpy(buf.text + index, server.usrs_online[i].name, 15);
            index = index + 16;
        }
    }
    //发送结果
    ret = send(recvfd, &buf, sizeof(buf), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    //将此文件描述符从忙文件描述符数组中移除
    server.workingfds[recvfd] = 0;
}

void RecieveUploadFiles(const int recvfd, const void *recvinfo)
{
    int ret, tmp;
    //接收文件的大小
    unsigned int filesize = 0;

    char upload_result;
    //用于创建相应的文件
    char create_file[256] = {0};

    //缓冲区
    struct info buf;

    //创建的新文件的文件指针
    FILE *recv_fp = NULL;

    memcpy(&buf, recvinfo, sizeof(buf));
    sprintf(create_file, "./usrsuploadfiles/%s", buf.text);
    printf("%s\n", create_file);
    // printf("%d\n", __LINE__);
    //创建文件
    recv_fp = fopen(create_file, "w");
    if (NULL == recv_fp)
    {
        ERRLOG("fopen");
        /**
         * @brief
         * 此处注意，返回前，应将此文件描述符从忙文件描述符数组中移除
         * 不然可能出现服务器崩溃的情况，当返回后，此文件描述符没
         * 从忙文件描述符数组中移除时，此文件描述符对应的客户端有新的
         * 请求到来时，将得不到处理，会出现死循环的情况，直至
         * 崩溃
         *
         */
        server.workingfds[recvfd] = 0;

        return;
    }

    //接收要接收的文件的大小
    ret = recv(recvfd, &filesize, sizeof(filesize), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }
    // printf("filesize is %d\n", filesize);

    //开始接收文件
    while (1)
    {
        char recvfile_buf[1024] = {0};

        //当剩余文件长度为0时，则传输结束，跳出循环
        if (0 == filesize)
        {
            printf("文件传输结束\n");
            break;
        }
        //接收文件并记录接收的字节数
        if (filesize > sizeof(recvfile_buf))
        {
            tmp = recv(recvfd, recvfile_buf, sizeof(recvfile_buf), 0);
        }
        else
        {
            tmp = recv(recvfd, recvfile_buf, filesize, 0);
        }
        if (-1 == tmp)
        {
            ERRLOG("recv");
        }
        //写入目标文件
        fwrite(recvfile_buf, tmp, 1, recv_fp);
        //更新文件剩余字节数
        filesize -= tmp;
    }

    //向客户端发送文件接收（上传）成功的消息
    upload_result = 's';
    ret = send(recvfd, &upload_result, sizeof(upload_result), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }
    // printf("发送了%d:%c\n", recvfd, upload_result);

    //关闭打开的文件指针
    fclose(recv_fp);
    //将此文件描述符从忙文件描述符数组中移除
    server.workingfds[recvfd] = 0;

    return;
}

void SendUploadFiles(const int recvfd, const void *recvinfo)
{
    int ret, tmp;
    //发送文件剩余字节数
    unsigned int filesize = 0;

    //中间生成文件files.txt的文件指针
    FILE *fp_name = NULL;
    //客户端要下载文件的文件指针
    FILE *fp_sendfile = NULL;

    char isexist;
    char sendresult;
    char filename[128] = {0};
    char tmp_send_file[256] = {0};

    //查看可下载文件目录下的所有文件，输出重定位到files.txt
    system("ls ./usrsuploadfiles > files.txt");
    //以只读方式打开files.txt
    fp_name = fopen("files.txt", "r");
    if (NULL == fp_name)
    {
        ERRLOG("fopen");
        /**
         * @brief
         * 此处注意，返回前，应将此文件描述符从忙文件描述符数组中移除
         * 不然可能出现服务器崩溃的情况，当返回后，此文件描述符没
         * 从忙文件描述符数组中移除时，此文件描述符对应的客户端有新的
         * 请求到来时，将得不到处理，会出现死循环的情况，直至
         * 崩溃
         *
         */
        server.workingfds[recvfd] = 0;

        return;
    }

    //获取files.txt文件字节数
    fseek(fp_name, 0, SEEK_END);
    filesize = ftell(fp_name);
    fseek(fp_name, 0, SEEK_SET);

    //将files.txt文件大小发送给客户端
    ret = send(recvfd, &filesize, sizeof(filesize), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    //将files.txt文件内容发送给客户端
    while (1)
    {
        char sendfile_buf[1024] = {0};

        if (0 == filesize)
        {
            break;
        }
        tmp = fread(sendfile_buf, 1, sizeof(sendfile_buf), fp_name);
        tmp = send(recvfd, sendfile_buf, tmp, 0);
        if (-1 == tmp)
        {
            ERRLOG("send");
        }
        filesize -= tmp;
    }
    //关闭中间文件files.txt文件指针并删除文件
    fclose(fp_name);
    system("rm files.txt");

    //接收客户端要下载文件的文件名
    ret = recv(recvfd, filename, sizeof(filename), 0);
    if (-1 == ret)
    {
        ERRLOG("recv");
    }

    //打开相应的文件
    sprintf(tmp_send_file, "./usrsuploadfiles/%s", filename);
    fp_sendfile = fopen(tmp_send_file, "r");
    if (NULL == fp_sendfile)
    {
        //若文件不存在，则发送'n'，并直接返回
        isexist = 'n';
        ret = send(recvfd, &isexist, sizeof(isexist), 0);
        {
            if (-1 == ret)
            {
                ERRLOG("send");
            }
        }
    }
    else
    {
        //若文件存在，则发送's'，并开始传送文件
        isexist = 'y';
        ret = send(recvfd, &isexist, sizeof(isexist), 0);
        {
            if (-1 == ret)
            {
                ERRLOG("send");
            }
        }

        //获取发送文件的文件大小（字节数）
        fseek(fp_sendfile, 0, SEEK_END);
        filesize = ftell(fp_sendfile);
        fseek(fp_sendfile, 0, SEEK_SET);

        //告知客户端要发送文件的大小
        ret = send(recvfd, &filesize, sizeof(filesize), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }

        // printf("filesize is %d\n", filesize);
        //开始发送文件
        while (1)
        {
            char sendfile_buf[1024] = {0};
            //文件读取完毕，跳出循环
            if (0 == filesize)
            {
                printf("文件传输结束\n");
                break;
            }
            //记录读入的字节数用作send函数发送时参数
            tmp = fread(sendfile_buf, 1, sizeof(sendfile_buf), fp_sendfile);
            //发送读入的内容并记录发送的字节数
            tmp = send(recvfd, sendfile_buf, tmp, 0);
            if (-1 == tmp)
            {
                ERRLOG("send");
            }
            //更新剩余字节数
            filesize -= tmp;
            // printf("tmp is %d\n", tmp);
            // printf("filesize is %d\n", filesize);
        }

        //向客户端发送文件发送成功的消息
        sendresult = 's';
        ret = send(recvfd, &sendresult, sizeof(sendresult), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
        fclose(fp_sendfile);
    }
    /**
     * @brief
     * 此处注意，返回前，应将此文件描述符从忙文件描述符数组中移除
     * 不然可能出现服务器崩溃的情况，当返回后，此文件描述符没
     * 从忙文件描述符数组中移除时，此文件描述符对应的客户端有新的
     * 请求到来时，将得不到处理，会出现死循环的情况，直至
     * 崩溃
     *
     */
    server.workingfds[recvfd] = 0;

    return;
}

void UpdateName(const int recvfd, const void *recvinfo)
{
    int ret;

    //接收呢称是否修改成功的结果
    char update_result;
    //存放执行的sql语句
    char sql[128] = {0};

    //发送接收消息的缓冲区
    struct info buf;

    memcpy(&buf, recvinfo, sizeof(buf));
    //打开数据库句柄
    //数据库操作前上锁
    LOCK(usrsdb_mutex);
    ret = sqlite3_open("./usrs/usrs.db", &ppdb);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_open");
    }
    //执行更新昵称的语句
    sprintf(sql, "update usr set name='%s' where id='%s';", buf.usr.name, buf.usr.id);
    ret = sqlite3_exec(ppdb, sql, NULL, NULL, NULL);
    //关闭数据库句柄
    sqlite3_close(ppdb);
    ppdb = NULL;
    //数据库操作后解锁
    UNLOCK(usrsdb_mutex);

    if (SQLITE_OK != ret)
    {
        //向客户端发送昵称修改失败的结果
        ERRLOG("sqlite3_exec");
        update_result = 'f';
        ret = send(recvfd, &update_result, sizeof(update_result), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    else
    {
        //向客户端发送昵称修改成功的结果
        update_result = 's';
        ret = send(recvfd, &update_result, sizeof(update_result), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    //将此文件描述符从忙文件描述符数组中移除
    server.workingfds[recvfd] = 0;

    return;
}

void RetrievePasswd(const int recvfd, const void *recvinfo)
{
    int ret;
    //存储数据库查询结果的行数和列数
    int row, col;

    //存放数据库查询的结果
    char **result;
    //存放执行的sql语句
    char sql[256] = {0};

    struct info buf;

    memcpy(&buf, recvinfo, sizeof(buf));
    //打开数据库句柄
    //数据库操作前上锁
    LOCK(usrsdb_mutex);
    ret = sqlite3_open("./usrs/usrs.db", &ppdb);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_open");
    }
    //将执行的sql语句放入sql数据中
    snprintf(sql, 256, "select passwd from usr where id='%s' and propasswd='%s';", buf.usr.id, buf.text);
    //执行sql语句并获取查询结果
    ret = sqlite3_get_table(ppdb, sql, &result, &row, &col, NULL);
    //关闭数据库句柄
    sqlite3_close(ppdb);
    ppdb = NULL;
    //数据库操作后解锁
    UNLOCK(usrsdb_mutex);

    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_get_table");
    }
    if (0 == row)
    {
        //若传出参数row=0，则说明没有查到结果，向客户端发送'f'
        memset(&buf, 0, sizeof(buf));
        buf.opt = U_RETRIEVE_PASSWD;
        buf.text[0] = 'f';

        ret = send(recvfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    else if (1 == row)
    {
        //若传出参数row=1，则说明查到了密码，向客户端发送密码
        memset(&buf, 0, sizeof(buf));
        buf.opt = U_RETRIEVE_PASSWD;
        strncpy(buf.usr.passwd, result[1], 13);

        ret = send(recvfd, &buf, sizeof(buf), 0);
        if (-1 == ret)
        {
            ERRLOG("send");
        }
    }
    //释放查询结果所占用的空间
    sqlite3_free_table(result);

    return;
}

void UpdatePasswd(const int recvfd, const void *recvinfo)
{
    int ret;
    //作为传出参数，用作查询结果的行数与列数
    int row, column;

    //传出参数，用于执行存放数据库查询结果的空间
    char **result;
    //服务器返回给客户端的修改密码的结果
    char update_result;
    //存放要执行的sql语句
    char sql[256] = {0};

    struct info buf;

    memcpy(&buf, recvinfo, sizeof(buf));
    //打开数据库句柄
    //数据库操作前上锁
    LOCK(usrsdb_mutex);
    ret = sqlite3_open("./usrs/usrs.db", &ppdb);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_open");
    }
    snprintf(sql, 256, "select passwd from usr where id='%s';", buf.usr.id);
    //执行sql语句
    ret = sqlite3_get_table(ppdb, sql, &result, &row, &column, NULL);
    if (SQLITE_OK != ret)
    {
        ERRLOG("sqlite3_exec");
    }

    do
    {
        if (0 != strcmp(result[1], buf.usr.passwd))
        {
            //若输入的原密码错误，则向客户端发送'f'
            //修改密码失败，说明客户端输入的参数有误，将消息发送给客户端
            update_result = 'f';
            ret = send(recvfd, &update_result, sizeof(update_result), 0);
            if (-1 == ret)
            {
                ERRLOG("send");
            }
            break;
        }
        else if (0 == strcmp(result[1], buf.text))
        {
            //若输入的原密码正确但新密码与原来的密码一样，则向客户端发送'e'
            //修改密码失败，客户端输入的新密码与原密码相同
            update_result = 'e';
            ret = send(recvfd, &update_result, sizeof(update_result), 0);
            if (-1 == ret)
            {
                ERRLOG("send");
            }
            break;
        }
        else
        {
            //若输入的原密码正确且新密码与原密码不同，则修改成功，向客户端发送's'
            //执行修改密码的sql语句
            memset(sql, 0, sizeof(sql));
            snprintf(sql, 256, "update usr set passwd='%s' where id='%s' and passwd='%s';", buf.text, buf.usr.id, buf.usr.passwd);
            ret = sqlite3_exec(ppdb, sql, NULL, NULL, NULL);
            if (SQLITE_OK != ret)
            {
                ERRLOG("sqlite3_exec");
            }
            //修改密码成功
            update_result = 's';
            ret = send(recvfd, &update_result, sizeof(update_result), 0);
            if (-1 == ret)
            {
                ERRLOG("send");
            }
            break;
        }
    } while (0);

    //释放查询结果所占用的空间
    sqlite3_free_table(result);
    //关闭数据库句柄并置空
    sqlite3_close(ppdb);
    ppdb = NULL;
    //数据库操作后解锁
    UNLOCK(usrsdb_mutex);
    //将此文件描述符从忙文件描述符数组中移除
    server.workingfds[recvfd] = 0;

    return;
}

void Admin(const int recvfd, const void *recvinfo)
{
    int ret, i;

    // 向管理员的操作的回应
    char admin_result;

    struct info buf;

    memcpy(&buf, recvinfo, sizeof(buf));
    for (i = 0; i <= server.login_maxfd; i++)
    {
        // 找到管理员进行操作的目的账号ID
        if (0 == strcmp(buf.usr.id, server.usrs_online[i].id))
        {
            while (1)
            {
                // 若目的账号正在忙，则过一会处理
                if (0 != server.workingfds[recvfd])
                {
                    usleep(500000);
                }
                else
                {
                    // 向目的客户端发送管理员操作的消息
                    ret = send(i, &buf, sizeof(buf), 0);
                    if (-1 == ret)
                    {
                        ERRLOG("send");
                    }
                    // 将结果发送给管理员
                    admin_result = 's';
                    ret = send(recvfd, &admin_result, sizeof(admin_result), 0);
                    if (-1 == ret)
                    {
                        ERRLOG("send");
                    }

                    return;
                }
            }
        }
    }
    // 没找到目的账号
    // 将结果发送给管理员
    admin_result = 'f';
    ret = send(recvfd, &admin_result, sizeof(admin_result), 0);
    if (-1 == ret)
    {
        ERRLOG("send");
    }

    return;
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