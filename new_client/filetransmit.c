/**
 * 有待完善，后续更新....
 */ 
#include "filetransmit.h"

void DownloadFile(client_t *client, const char *filename, long filesize)
{
    int filefd, ret;
    char filepath[BUF_SIZE];
    void *fileptr;
    long recvsize = 0;

    snprintf(filepath, BUF_SIZE, "%s/mydownloadfiles/%s", client->myinfo.id, filename);
    filefd = open(filepath, O_CREAT | O_TRUNC | O_RDWR, 0664);
    if (filefd == -1)
    {
        fprintf(stderr, "文件创建失败.\n");
        return;
    }

    // 将文件空间扩展为filesize
    ret = ftruncate(filefd, filesize);
    if (ret == -1)
    {
        fprintf(stderr, "文件扩展失败.\n");
        return;
    }

    // 将文件映射到内存上
    fileptr = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, filefd, 0);
    if (fileptr == (void *)-1)
    {
        fprintf(stderr, "文件映射内存失败.\n");
        return;
    }

    // 向服务端确认开始接收文件
    downloadfile_info sendinfo_recv;
    
    sendinfo_recv.cmd = FILE_DOWNLOAD;
    strcpy(sendinfo_recv.filename, filename);
    printf("与服务器建立连接...\n");
    if (Connect(client) != SUCCESS)
    {
        return;
    }
    send(client->sockfd_file, &sendinfo_recv, sizeof(sendinfo_recv), 0);

    printf("开始接收文件...\n");
    fileinfo myinfo;
    // 开始接受文件
    int realrecv = 0;
    while (recvsize != filesize)
    {
        realrecv = recv(client->sockfd_file, &myinfo, sizeof(myinfo), MSG_DONTWAIT);
        if (realrecv < 0)
        {
            // 判断是否发生了错误
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                // 未发生错误
                // 睡眠0.1秒并直接返回
                // printf("recv\n"); // test
                usleep(100000);
                continue;
            }
            else
            {
                // 发生了错误
                fprintf(stderr, "recv error:%d\n", client->sockfd_file);
            }
        }
        else if (realrecv == 0)
        {
            printf("连接中断...\n");
            exit(-1);
        }
        // 更新累计数据长度
        recvsize += myinfo.filedatalength;
        memcpy(fileptr + FILEBLOCK_MAXSIZE * myinfo.fileorder + FILE_BUF_SIZE * myinfo.file_inblocknumber, 
        myinfo.filebuffer, myinfo.filedatalength);
        printf("recving.\n");
    }

    printf("文件接收完毕...\n");
    printf("请到%s/mydownloadfiles目录查看.\n", client->myinfo.id);
    // 文件接收完毕
    close(filefd);
    // 关闭连接
    
    // 取消映射
    munmap(fileptr, filesize);
}

int Connect(client_t *client)
{
    int ret;

    // 建立传输文件的连接
    // 创建套接字
    client->sockfd_file = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sockfd_file == -1)
    {
        fprintf(stderr, "套接字创建失败.\n");
        return FAILURE;
    }

    // 填充网络信息结构体
    struct sockaddr_in serveraddr;
    socklen_t serveraddr_len = sizeof(serveraddr);

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(client->server_ip);
    serveraddr.sin_port = htons(client->server_file_port);

    // 连接至服务器
    ret = connect(client->sockfd_file, (struct sockaddr *)&serveraddr, serveraddr_len);
    if (ret == -1)
    {
        fprintf(stderr, "连接服务器失败\n");
        return FAILURE;
    }

    // 设置套接字为非阻塞状态
    int flags;
    flags = fcntl(client->sockfd_file, F_GETFL);
    flags |= O_NONBLOCK;
    ret = fcntl(client->sockfd_file, F_SETFL, flags);
    if (ret == -1)
    {
        printf("设置非阻塞模式失败.\n");
        return FAILURE;
    }

    return SUCCESS;
}