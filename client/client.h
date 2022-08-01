#ifndef _CLIENT_H_
#define _CLIENT_H_
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>

//打印出错误信息与发生位置
#define ERRLOG(errmsg)                                          \
    do                                                          \
    {                                                           \
        perror(errmsg);                                         \
        printf("%s - %s - %d\n", __FILE__, __func__, __LINE__); \
    } while (0)
#define FAILURE -1
#define SUCCESS 0
//接收缓冲区
#define BUF_MAXSIZE 128

//进行的操作
enum option
{
    U_LOGIN = 1,
    U_REGISTER,
    U_CHATHALL,
    U_QUITCHAT,
    U_PRIVATE_CHAT,
    U_VIEW_ONLINE,
    U_UPLOAD_FILE,
    U_DOWN_FILE,
    U_UPDATE_NAME,
    U_RETRIEVE_PASSWD,
    U_UPDATE_PASSWD,
    ADMIN_BANNING,
    ADMIN_DIS_BANNING,
    ADMIN_EXITUSR
    /**
     * opt = U_LOGIN --- 登录
     * opt = U_REGISTER --- 注册
     * opt = U_CHATHALL --- 聊天大厅
     * opt = U_QUITCHAT --- 退出聊天大厅
     * opt = U_PRIVATE_CHAT --- 客户端间私聊
     * opt = U_VIEW_ONLINE --- 查看在线用户
     * opt = U_UPLOAD_FILE --- 上传文件
     * opt = U_DOWN_FILE --- 下载文件
     * opt = U_UPDATE_NAME --- 修改昵称
     * opt = U_RETRIEVE_PASSWD --- 找回密码
     * opt = U_UPDATE_PASSWD --- 修改密码
     *
     *
     * 管理员操作
     * opt = ADMIN_BANNING
     * opt = ADMIN_DIS_BANNING
     * opt = ADMIN_EXITUSR
     */
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

//存放客户端基本参数结构体
struct client_t
{
    //创建的套接字
    int sockfd;
    //聊天权限标志位
    int disabledchat;
    // 客户端状态，置0表示正常运转，置1时停止运行退出程序
    unsigned int client_shutdown;
    //客户端自身信息
    struct usrinfo myinfo;
};

//客户端选择的操作以及所需传递的信息
struct info
{
    /**
     * @brief 传递的消息正文
     * opt = U_LOGIN --- 登录
     * opt = U_REGISTER --- 注册
     * opt = U_CHATHALL --- 聊天大厅
     * opt = U_QUITCHAT --- 退出聊天大厅
     * opt = U_PRIVATE_CHAT --- 客户端间私聊
     * opt = U_VIEW_ONLINE --- 查看在线用户
     * opt = U_UPLOAD_FILE --- 上传文件
     * opt = U_DOWN_FILE --- 下载文件
     * opt = U_UPDATE_NAME --- 修改昵称
     * opt = U_RETRIEVE_PASSWD --- 找回密码
     * opt = U_UPDATE_PASSWD --- 修改密码
     *
     *
     * 管理员操作
     * opt = ADMIN_BANNING
     * opt = ADMIN_DIS_BANNING
     * opt = ADMIN_EXITUSR
     */
    //用户信息
    struct usrinfo usr;
    //消息正文
    char text[BUF_MAXSIZE];
    /**
     * @brief 标志客户端进行了什么操作
     * 1 --- 登录
     * 2 --- 注册
     * 3 --- 进入聊天大厅
     * 4 --- 退出聊天大厅
     * 5 --- 在聊天大厅内进行私聊
     * 6 --- 查看在线用户
     * 7 --- 客户端上传文件
     * 8 --- 客户端下载文件
     * 9 --- 修改昵称
     * 10 --- 用户找回密码
     * 11 --- 修改密码
     *
     * 管理员操作
     * 12 --- 对客户端禁言
     * 13 --- 对客户端解除禁言
     * 14 --- 对客户端踢出聊天室
     */
    enum option opt;
};

/**
 * @brief
 * 初始化动画
 *
 * 无参数，无返回值
 */
void StartAnimation();

/**
 * @brief
 * 连接服务器
 *
 * 无参数
 * @return int 连接成功返回0，失败返回-1
 */
int ConnectServer();

/**
 * @brief
 * 登录菜单
 *
 * 无参数，无返回值
 */
void LoReMenu();

/**
 * @brief
 * Login函数
 *
 * 进行账号登录
 *
 * 用户信息结构体为登录时输入的账号和密码，昵称无内容
 * 消息正文无内容
 *
 * 若Login函数收到's'，则说明登录成功
 * 用户信息结构体为登录用户的服务器端所存信息
 *
 * 若Login函数收到'f'账号或密码错误
 *
 * 若Login函数收到'l'，说明账号当前处于在线状态
 * 登录成功返回0，否则返回-1
 *
 * 无参数
 * @return int 登录成功返回0，失败返回-1
 */
int Login();

/**
 * @brief
 * Register函数
 *
 * 注册新账号
 *
 * 用户信息结构体为注册时输入的账号、密码和昵称
 * 消息正文无内容
 *
 * 若Register函数收到's'，则说明注册成功
 *
 * 若Register函数收到'f'，则说明注册失败，账号已存在
 *
 * 注册成功返回0，否则返回-1
 *
 * 无参数
 * @return int 注册成功返回0，失败返回-1
 */
int Register();

/**
 * @brief
 * 登录后的选择菜单
 *
 * 无参数，无返回值
 */
void Menu();

/**
 * @brief
 * ChatHall函数
 * 聊天大厅正常发送消息时，所有在聊天大厅内的其他用户都可见
 * 当进入聊天大厅时，即开始记录聊天信息到聊天记录文件中
 * 可以通过输入'1'退出聊天大厅
 * 可以通过输入'2'进行私聊操作
 * 可以通过输入'3'快速发送常用话语
 * 可以通过输入'4'快速发送表情
 *
 * 无参数，无返回值
 */
void ChatHall();

/**
 * @brief
 * 线程执行函数
 * 用于接收其他用户发送的消息
 * 当进入聊天大厅时，即开始记录聊天信息到聊天记录文件中
 *
 *
 * @param arg 无意义
 * @return void* 无返回值
 */
void *RecvFromHall(void *arg);

/**
 * @brief
 * 用户信息结构体和消息正文无内容
 * 当退出聊天大厅时，QuitChat函数将操作码option=5，然后发送给服务器
 *
 * 无参数，无返回值
 */
void QuitChat();

/**
 * @brief
 * PrivateChat函数
 * 可以与其他客户端进行私聊（格式：用户ID-消息）
 * 输入'2'进行私聊，输入'2'后，再通过输入固定格式输入信
 * 息（目标用户ID-消息内容），输入完信息进行打包发送给服务器后，
 * 服务器进行转发
 *
 * 无参数，无返回值
 */
void PrivateChat();

/**
 * @brief
 * 向聊天大厅中发送常用话语
 *
 * 无参数，无返回值
 */
void SendComm();

/**
 * @brief
 * 常用话语表
 * 当参数为常用话语表下标时
 * 返回值为相应的常用话语
 *
 * 当参数不在常用话语表数组下标内时
 * 打印常用话语表
 * 返回值为空
 */
char *CommonWords(const int choice);

/**
 * @brief
 * 快速向聊天大厅发送表情
 *
 * 无参数，无返回值
 */
void SendEmoji();

/**
 * @brief
 * 表情表
 * 当参数为表情表下标时
 * 返回值为相应的表情
 *
 * 当参数不在表情表数组下标内时
 * 打印表情表
 * 返回值为空
 */
char *Emojis(const int choice);

/**
 * @brief
 * ViewOnlineusers函数
 * 用户信息结构体无内容
 * ViewOnlineusers函数接收到的消息正文为在线用户的账号和昵称
 * 账号与昵称依次间隔一个'\0'
 * 账号长度为6，昵称长度为15
 *
 * 无参数，无返回值
 */
void ViewOnlineusers();

/**
 * @brief
 * 查看聊天记录文件
 *
 * 无参数，无返回值
 */
void ViewChattingRecords();

/**
 * @brief
 * UploadFiles函数
 * 上传文件到服务器
 * 先输入文件名，若文件存在，则开始上传
 * 若上传成功，函数内部会收到服务器的's'消息
 *
 * 无参数无返回值
 */
void UploadFiles();

/**
 * @brief
 * DownloadFiles函数
 * 下载文件
 * 用户信息结构体不内容
 * DownloadFIles函数先向服务器请求下载文件，服务器返回可供下
 * 载文件的文件名，然后输入要下载文件的文件名，发送给服务器，
 * 等待服务器确认文件是否存在，若存在，则开始接收文件
 * 接收完文件后，等待服务器确认，若收到服务器发送的's'，则下载成功
 * 若文件不存在，则直接返回
 *
 * 无参数无返回值
 */
void DownloadFiles();

/**
 * @brief
 * UpdateName函数
 * 修改昵称
 * 用户信息结构体为要修改昵称的账号的ID和要修改的新昵称，密码项为空
 * 消息正文无内容
 * UpdateName函数先检查要修改的新昵称是否符合昵称的格式，若是不符
 * 合，则需要一直输入直至符合，然后检查输入的新昵称与当前昵称是否一样，
 * 若是一样，则直接返回，否则将消息打包好发送给服务器
 * 然后接收服务器昵称是否修改成功的结果，若是接收到's'，则说明昵称
 * 修改成功，若收到'f'，则说明昵称修改失败
 *
 * 无参数无返回值
 */
void UpdateName();

/**
 * @brief
 * RetrievePasswd函数
 * 找回密码
 * 用户信息结构体为找回密码的账号ID，其他项无内容，消息正文为
 * 密保问题的回答，消息打包好后发送给服务器等待服务器的回应，
 * RetrievePasswd函数若是收到一个'f'，则说明密保问题回答错
 * 误，密码找回失败
 * 否则密码找回成功，密码在用户信息结构体的密码项
 *
 * 无参数无返回值
 */
void RetrievePasswd();

/**
 * @brief
 * UpdatePasswd函数
 * 修改密码
 * 用户信息结构体为要修改密码的账号ID和原密码，昵称项无内容，
 * 消息正文为输入的新的密码
 * UpdatePasswd函数将打包好的消息发送给服务器，然后等待接收
 * 请求修改密码的结果，若收到'f'，则修改密码失败
 * 若收到's'，则修改密码成功
 *
 * 无参数无返回值
 */
void UpdatePasswd();

/**
 * @brief
 * 仅限于管理员操作
 * 管理员对其他用户进行禁言或踢出聊天室
 *
 * 无参数无返回值
 */
void admin();

/**
 * @brief
 * 仅限于管理员操作
 * 对其他用户禁言
 * 用户信息结构体包含被禁言的账号ID，其他项无内容，消息正文无内容
 * 将消息打包好后直接发送给服务器，由服务器转发给目标客户端即可，然
 * 后等待接收服务器发来的操作是否成功的消息
 * 若接收到's'，则说明操作成功
 * 否则说明操作失败
 *
 * @param id 被禁言的用户的账号ID
 */
void BanningSpeaking(const char *id);

/**
 * @brief
 * 仅限于管理员操作
 * 对其他用户解除禁言
 * 用户信息结构体包含被解除禁言的账号ID，其他项无内容，消息正文无
 * 内容将消息打包好后直接发送给服务器，由服务器转发给目标客户端即
 * 可，然后等待接收服务器发来的操作是否成功的消息
 * 若接收到's'，则说明操作成功
 * 否则说明操作失败
 *
 *
 * @param id 被解除禁言的用户的账号ID
 */
void DisabledBanningSpeaking(const char *id);

/**
 * @brief
 * 仅限于管理员操作
 * 将其他用户踢出聊天室
 * 用户信息结构体包含被踢出聊天室的账号ID，其他项无内容，消息正文
 * 无内容将消息打包好后直接发送给服务器，由服务器转发给目标客户端
 * 即可，然后等待接收服务器发来的操作是否成功的消息
 * 若接收到's'，则说明操作成功
 * 否则说明操作失败
 *
 * @param id 被踢出聊天室的用户的账号ID
 */
void ExitChatroom(const char *id);

/**
 * @brief Get the Time object
 * 获取当前系统时间
 *
 * 无参数
 * @return char* 以字符串形式返回当前系统时间，需释放空间
 */
char *GetTime();

/**
 * @brief
 * 退出程序
 *
 * 无参数，无返回值
 */
void Exit();

#endif