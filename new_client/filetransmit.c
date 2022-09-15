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
    uploadfile_info sendinfo_recv;
    
    sendinfo_recv.cmd = FILE_DOWNLOAD;
    strcpy(sendinfo_recv.filename, filename);
    if (Connect(client) != SUCCESS)
    {
        return;
    }
    send(client->sockfd_file, &sendinfo_recv, sizeof(sendinfo_recv), 0);

    fileinfo myinfo;
    // 开始接受文件
    int realrecv = 0;
    while (recvsize >= filesize)
    {
        realrecv = recv(client->sockfd_file, &myinfo, sizeof(myinfo), 0);
        recvsize += realrecv;
        memcpy(fileptr + FILEBLOCK_MAXSIZE * myinfo.fileorder + FILE_BUF_SIZE * myinfo.file_inblocknumber, 
        myinfo.filebuffer, myinfo.filedatalength);
    }

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

    return SUCCESS;
}