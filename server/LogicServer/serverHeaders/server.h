#ifndef _SERVER_H_
#define _SERVER_H_
/**
 * @file server
 * @author xu (2280793957@qq.com)
 * @brief epoll+多线程轮询服务器
 * @date 2022-03-30
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <errno.h>

#include "../threadpool/threadpool.h"
#include "clientoptions.h"
#include "userinfo.h"
#include "userlinklist.h"
#include "info.h"

//打印出错误信息与发生位置
#define ERRLOG(errmsg)                                          \
    do                                                          \
    {                                                           \
        perror(errmsg);                                         \
        printf("%s - %s - %d\n", __FILE__, __func__, __LINE__); \
    } while (0)

#define FAILURE -1
#define SUCCESS 0
//客户端最大连接个数

#define CLIENT_MAXSIZE 1024

//线程互斥量上锁解锁操作
#define LOCK(mutex) pthread_mutex_lock(&(mutex))
#define UNLOCK(mutex) pthread_mutex_unlock(&(mutex))
#define TRYLOCK(mutex) pthread_mutex_trylock(&(mutex))

//服务器参数
typedef struct server_t
{
    // 创建的套接字
    int sockfd;
    // 创建epoll对象
    int epfd;

    // 存放在忙的文件描述符，保证一个文件描述符在只能同时被一个recv函数阻塞等待接收数据
    int workingfds[CLIENT_MAXSIZE];
    // 存放进入聊天大厅的用户
    int usrs_chathall[CLIENT_MAXSIZE];

    // 为验证账号是否在聊天大厅中的最大循环次数
    int chat_maxfd;

    // 标识当前服务器是否处于关闭状态
    int shutdown;

    // 存放发生事件的events数组
    struct epoll_event events[CLIENT_MAXSIZE];

    // 存放在线的用户的信息
    // 客户端的文件描述符即为存放客户端信息数组的下标
    userlinklist *usrs_online;
    pthread_mutex_t usrs_online_mutex;

    //创建的线程池
    threadpool *pool;

    //数据库句柄
    sqlite3 *usrsdb;
    //数据库操作互斥量
    pthread_mutex_t usrsdb_mutex;

    //文件指针（用于保存所有人的聊天信息）
    FILE *ChatFp;
    // 文件指针操作互斥量
    pthread_mutex_t ChatFp_mutex;
}server_t;

/**
 * @brief
 * 初始化服务器基本参数
 * server : 服务器结构体
 * @return 成功返回0 失败返回-1
 */
int InitServerOptions(server_t *server);

/**
 * @brief
 * 启动epoll服务器
 * 无参数
 * @return 成功返回0 失败返回-1
 */
int InitServer();

/**
 * @brief
 * 当捕捉到SIGINT信号时，执行此函数关闭epoll服务器
 * 无参数
 * 无返回值
 */
void CloseServer();

/**
 * @brief
 * 负责处理客户端请求的连接
 *
 * 参数无意义
 */
void AcceptClient(const int recvfd, const void *recvinfo);

/**
 * @brief
 * 负责接收并处理客户端的请求
 *
 * 参数无意义
 */
void TransMsg(const int recvfd, const void *recvinfo);

/**
 * 负责处理在线用户的心跳检测
 * 向所有在线用户发送心跳包
 */
void HeartBeatSend(const int recvfd, const void *recvinfo);

/**
 * 处理接收到的客户端回应的心跳包
 * recvfd:回应的客户端的套接字描述符
 * recvinfo:无意义
 */
void HeartBeatHandler(const int recvfd, const void *recvinfo);

/**
 * @brief
 * 当用户下线时，清除用户的在线的相关信息
 *
 * @param recvfd 下线用户的文件描述符
 * @param recvinfo 无意义，为NULL
 */
void ClearClient(const int recvfd, const void *recvinfo);

/**
 * @brief
 * Login函数
 * 用户信息结构体接收账号和密码，消息正文无内容
 *
 * 根据接收到的账号密码查询数据库数据
 * 若查询到，则说明登录成功，向客户端发送结果
 * 用户信息结构体为登录用户的账号和昵称，密码项为空
 *
 * 若查询到，且账号处于离线状态，向客户端发送结果
 * 用户信息结构体为空
 * 消息正文为's'
 *
 * 若没有查询到，则说明登录失败，向客户端发送结果
 * 用户信息结构体为空
 * 消息正文为'f'
 *
 * 若查询到，但账号处于在线状态，向客户端发送结果
 * 用户信息结构体为空
 * 消息正文为'l'
 *
 * @param recvfd 客户端的文件描述符
 * @param recvinfo 客户端请求登录的相关信息
 */
void Login(const int recvfd, const void *recvinfo);

/**
 * @brief
 * Register函数
 * 用户信息结构体接收账号、密码和昵称，消息正文为密保的答案
 *
 * 根据接收到的账号密码查询数据库数据，向客户端发送结果
 * 若没有查询到数据，则说明账号不存在，可注册
 * 用户信息结构体为空
 * 消息正文为's'
 *
 * 若查询到数据，则说明账号已存在，不可注册
 * 用户信息结构体为空
 * 消息正文为'f'
 *
 * @param recvfd 接收到的发消息方的文件描述符
 * @param recvinfo 发消息方发送的消息内容
 */
void Register(const int recvfd, const void *recvinfo);

/**
 * @brief
 * ChatHall函数
 * 接收到客户端的消息后转发给其他在聊天大厅内的用户
 * 用户信息结构体为进入聊天大厅的用户的账号和昵称，密码项为空
 * 消息正文为用户发送的信息内容
 *
 * 只有进入聊天大厅的客户端才能收到聊天大厅中其他客户端发送的消息
 *
 * @param recvfd
 * @param recvinfo
 */
void ChatHall(const int recvfd, const void *recvinfo);

/**
 * @brief
 * Quit函数
 * 用户信息结构体为要退出聊天大厅的用户的账号和昵称，密码项为空
 * 正文无内容
 * 把目标用户从聊天大厅用户表中移除
 *
 * @param recvfd 退出聊天大厅的客户端的文件描述符
 * @param recvinfo 无意义，为NULL
 */
void QuitChat(const int recvfd, const void *recvinfo);

/**
 * @brief
 * PrivateChat函数
 * 接收到客户端的私聊请求后，先查找目标用户是否在聊天大厅内
 * 若在，则进行消息转发
 * 用户信息结构体是发消息方的账号和昵称，密码项为空
 * 消息正文开头存的是 收消息方账号-消息内容
 *
 * @param recvfd 发送方的文件描述符
 * @param recvinfo 无意义，为NULL
 */
void PrivateChat(const int recvfd, const void *recvinfo);

/**
 * @brief
 * ViewOnlineusers函数
 * 用户信息结构体无内容
 * 消息正文为在线用户的账号和昵称
 * 账号与昵称依次间隔一个'\0'
 * 账号长度为6，昵称长度为15
 *
 * @param recvfd 客户端的文件描述符
 * @param recvinfo 客户端发送的消息内容
 */
void ViewOnlineusers(const int recvfd, const void *recvinfo);

/**
 * @brief
 * RecieveUploadFiles函数
 * 用户信息结构体无内容
 * 第一次接收到的消息正文内容为文件名
 * 先创建相应的文件，接收文件的大小
 * 然后开始接收文件内容
 * 当接收到的消息正文无内容时，则说明文件
 * 传输结束，将文件上传成功的消息告知客户端
 *
 * @param recvfd 上传文件的客户端的文加描述符
 * @param recvinfo 上传的文件内容
 */
void RecieveUploadFiles(const int recvfd, const void *recvinfo);

/**
 * @brief
 * 向客户端发送可供下载文件的文件名
 *
 * @param recvfd 请求下载文件的客户端的文件描述符
 * @param recvinfo
 */
void SendFileName(const int recvfd, const void *recvinfo);

/**
 * 向客户发送所要下载文件是否存在的结果
 */
void SendFileIsExist(const int recvfd, const void *recvinfo);

/**
 * @brief
 * UpdateName函数
 * 用户信息结构体为要修改昵称的账号的ID和要修改的新昵称，密码项为空
 * 消息正文无内容
 * UpdateName函数若是接收到了客户端的修改新昵称的请求，则说明在客户端
 * 经过了检验，可以修改，直接修改即可，若修改成功向客户端发送's'，
 * 否则发送'f'
 *
 * @param recvfd 请求的客户端的文件描述符
 * @param recvinfo 接收到的打包好的消息，其中包括要修改昵称的用户的账号ID和新昵称
 */
void UpdateName(const int recvfd, const void *recvinfo);

/**
 * @brief
 * RetrievePasswd函数
 * * 用户信息结构体为找回密码的用户的账号ID，密码项和昵称项
 * 为空，消息正文为用户对于密保问题的回答
 * RetrievePasswd函数根据客户端输入的信息查询密码
 * 若是没有查找到
 * 结果，则说明问题回答失败，向客户端发送找回密码失败的消息，消息为
 * 用户信息结构体无内容，消息正文开头'f'
 * 若是查找到
 * 结果，则说明问题回答成功，向客户端发送密码，消息为
 * 用户信息结构体密码项为密码，其他项无内容，消息正文无内容
 *
 * @param recvfd 请求的客户端的文件描述符
 * @param recvinfo 接收到的打包好的消息，其中包括找回密码的用户的账号ID和密保问题的回答
 */
void RetrievePasswd(const int recvfd, const void *recvinfo);

/**
 * @brief
 * UpdatePasswd函数
 * 接收到的消息，用户信息结构体为用户的账号ID和原密码，消息正文为
 * 新的密码，根据收到的消息执行sql语句，若是修改成功，则向客户端
 * 发送's'，若是修改失败，则向客户端发送'f'
 *
 * @param recvfd 请求的客户端的文件描述符
 * @param recvinfo 接收到的打包好的消息，其中包括修改密码的用户的账号ID和原密码
 */
void UpdatePasswd(const int recvfd, const void *recvinfo);

/**
 * @brief
 * opt = ADMIN_BANNING
 * 接收到此消息后，直接由Admin函数转发给相应客户端，由客户端
 * 自行处理即可
 * 若成功转发给了目的客户端，则向管理员方发送's'
 * 否则发送'f'
 *
 * opt = ADMIN_DIS_BANNING
 * 接收到此消息后，直接由Admin函数转发给相应客户端，由客户端
 * 自行处理即可
 * 若成功转发给了目的客户端，则向管理员方发送's'
 * 否则发送'f'
 *
 * opt = ADMIN_EXITUSR
 * 接收到此消息后，直接由Admin函数转发给相应客户端，由客户端
 * 自行处理即可
 * 若成功转发给了目的客户端，则向管理员方发送's'
 * 否则发送'f'
 *
 * @param recvfd 管理员登录客户端对应的套接字文件描述符
 * @param recvinfo 接收到的打包好的消息，其中包括被禁言用户的账号ID
 */
void Admin(const int recvfd, const void *recvinfo);

/**
 * @brief Get the Time object
 * 获取当前系统时间
 *
 * 无参数
 * @return char* 以字符串形式返回当前系统时间，需释放空间
 */
char *GetTime();

#endif