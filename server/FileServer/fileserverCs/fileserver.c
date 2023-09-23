#include "../fileserverHeaders/fileserver.h"

int InitFileServer(fileserver_t *fileserver, const char *ip, unsigned short port)
{
    int ret;

    // 获取套接字描述符
    ret = GetSocketfd();
    if (ret == FAILURE)
        return FAILURE;
    fileserver->sockfd = ret;
    fileserver->ip = ip;
    fileserver->port = port;

    fileserver->shutdown = 0;

    return SUCCESS;
}

int StartReadyFileServer(fileserver_t *fileserver)
{
    struct sockaddr_in serveraddr;
    socklen_t serveraddr_len = sizeof(serveraddr);
    int ret, flag;

    // 填充网络信息结构体
    memset(&serveraddr, 0, serveraddr_len);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(fileserver->ip);
    serveraddr.sin_port = htons(fileserver->port);

    int opt = 1;
    // 设置地址可以重复绑定
    ret = setsockopt(fileserver->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        fprintf(stderr, "setsockopt reuseaddr failed.\n");
        return FAILURE;
    }

    // 绑定套接字文件描述符和网络信息结构体
    ret = bind(fileserver->sockfd, (struct sockaddr *)&serveraddr, serveraddr_len);
    if (ret == -1)
    {
        fprintf(stderr, "bind sockfd and sockaddr failed. errno number : %d\n", errno);
        return FAILURE;
    }

    // 设置套接字由主动连接状态变为被动连接状态
    // 未连接队列与已连接队列长度之和不可超过10
    ret = listen(fileserver->sockfd, 10);
    if (ret == -1)
    {
        fprintf(stderr, "listen failed.\n");
        return FAILURE;
    }

    // 设置套接字为非阻塞
    flag = fcntl(fileserver->sockfd, F_GETFL);
    flag |= O_NONBLOCK;
    ret = fcntl(fileserver->sockfd, F_SETFL, flag);
    if (ret == -1)
    {
        fprintf(stderr, "fileserver set nowait failed.\n");
        return FAILURE;
    }

    return SUCCESS;
}

void Start(fileserver_t *fileserver)
{
    int newfd, ret;
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_len = sizeof(clientaddr);

    pid_t pid;

    printf("Running...\n");
    // 开始监听连接
    while (!fileserver->shutdown)
    {
        newfd = accept(fileserver->sockfd, (struct sockaddr *)&clientaddr, &clientaddr_len);
        if (newfd < 0)
        {
            if (errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EWOULDBLOCK || errno == EINTR)
            {
                // 定时回收子进程，若无则立即返回
                waitpid(-1, NULL, WNOHANG);
                // printf("time to waitpid.\n"); // test

                // 未监听到连接
                // 睡眠0.1秒并返回继续循环
                usleep(100000);
                continue;
            }
            // 发生异常错误
            fprintf(stderr, "accept error.\n");
            exit(-1);
        }
        printf("recieve client connection.\n");
        // 创建新的进程为客户端服务
        pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "fork error");
        }
        else if (pid > 0)
        { // 父进程
            // 继续回到循环开始监听
            continue;
        }
        else
        { // 子进程
            ClientService(fileserver, newfd);
        }
    }
}

int GetSocketfd()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        fprintf(stderr, "create socket failed.\n");
        return FAILURE;
    }

    return sockfd;
}

void ClientService(fileserver_t *fileserver, int fd)
{
    int ret;

    file_normalinfo myinfo;

    while (!fileserver->shutdown)
    {
        ret = recv(fd, &myinfo, sizeof(myinfo), MSG_DONTWAIT);
        if (ret < 0)
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
                fprintf(stderr, "recv error:%d\n", fd);
                exit(-1);
            }
        }
        else if (ret == 0)
        {
            printf("client:%d is offline.\n", fd);
            // 父进程会回收子进程资源
            exit(0);
        }
        else
        {
            // 处理客户端相应请求
            switch (myinfo.cmd)
            {
            case FILE_DOWNLOAD:
                SendFile(&myinfo, fd);
                break;

            default:
                break;
            }
        }
    }

    // 结束子进程，父进程会定时回收子进程资源
    exit(0);
}

void SendFile(file_normalinfo *recvinfo, int fd)
{
    // 获取相应文件名
    downloadfile_info *myinfo = (downloadfile_info *)recvinfo;
    // 文件总大小
    long filesize, sendsize;
    // 文件块数     文件描述符      文件块内编号
    int filetotalnumber, filefd, file_inblocknumber = 0;
    char filepath[BUF_SIZE], buf[3 * BUF_SIZE];
    void *fileptr;

    snprintf(filepath, BUF_SIZE, "./usrsuploadfiles/%s", myinfo->filename);
    filefd = open(filepath, O_RDWR);
    if (filefd == -1)
    {
        fprintf(stderr, "file open failed.\n");
        // 错误处理...
    }
    // 计算文件大小
    filesize = lseek(filefd, 0, SEEK_END);
    // 关闭文件描述符
    close(filefd);

    // 根据文件大小创建相应数量的进程
    filetotalnumber = filesize / FILEBLOCK_MAXSIZE;
    printf("%d\n", filetotalnumber);

    if (filetotalnumber == 0)
    { // 文件不需要分块，直接发就行了
        filefd = open(filepath, O_RDWR);
        if (filefd == -1)
        {
            fprintf(stderr, "file open failed.\n");
            // 错误处理...
        }

        // 填充数据包
        struct fileinfo sendinfo;
        sendinfo.cmd = FILE_DOWNLOAD;
        sendinfo.filesize = filesize;
        sendinfo.filetotalnumber = 1;
        sendinfo.fileorder = 0;
        sendinfo.filerealsize = filesize;

        // 映射文件到内存上
        fileptr = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, filefd, 0);
        if (fileptr == (void *)-1)
        {
            fprintf(stderr, "mmap file failed.\n");
            // 错误处理...
        }

        // 开始发送
        sendsize = filesize;
        file_inblocknumber = 0;
        while (sendsize >= FILE_BUF_SIZE)
        {
            memcpy(sendinfo.filebuffer, fileptr + filesize - sendsize, FILE_BUF_SIZE);
            sendinfo.filedatalength = FILE_BUF_SIZE;
            sendinfo.file_inblocknumber = file_inblocknumber++;
            // 向客户端发送数据
            send(fd, &sendinfo, sizeof(sendinfo), 0);
            // 更新剩余大小
            sendsize -= FILE_BUF_SIZE;
        }
        // 发送最后一个数据包
        sendinfo.filedatalength = sendsize;
        sendinfo.file_inblocknumber = file_inblocknumber++;
        memcpy(sendinfo.filebuffer, fileptr + filesize - sendsize, sendsize);
        send(fd, &sendinfo, sizeof(sendinfo), 0);

        // 文件发送完毕
        // close(fd);
        close(filefd);
        munmap(fileptr, filesize);
    }
    else
    {
        // 后续更新...

        // 传输大文件，需要分块
        // 利用split命令将文件分块
        memset(buf, 0, sizeof(buf));
        snprintf(buf, 3 * BUF_SIZE, "split -b %dm %s -d -a 2 %s", FILEBLOCK_MAXSIZE_MB, filepath, filepath);
        system(buf);
        // 开始创建多进程传输数据块
        int i, pid;
        for (i = 0; i < filetotalnumber; i++)
        {
            if ((pid = fork()) == -1)
            {
                fprintf(stderr, "fork failed.\n");
            }
            else if (pid == 0)
            {
                break;
            }
        }
        if (i == filetotalnumber)
        { // 父进程
            snprintf(filepath, BUF_SIZE, "./usrsuploadfiles/%s0%d", myinfo->filename, i);
            printf("%s\n", filepath); // test
            filefd = open(filepath, O_RDWR);
            if (filefd == -1)
            {
                fprintf(stderr, "file open failed.\n");
                // 错误处理...
            }

            // 填充数据包
            struct fileinfo sendinfo;
            sendinfo.cmd = FILE_DOWNLOAD;
            sendinfo.filesize = filesize;
            sendinfo.filetotalnumber = filetotalnumber;
            sendinfo.fileorder = i;
            sendinfo.filerealsize = filesize % FILEBLOCK_MAXSIZE;

            // 映射文件到内存上
            fileptr = mmap(NULL, sendinfo.filerealsize, PROT_READ | PROT_WRITE, MAP_SHARED, filefd, 0);
            if (fileptr == (void *)-1)
            {
                fprintf(stderr, "mmap file failed.\n");
                // 错误处理...
            }

            // 开始发送
            sendsize = sendinfo.filerealsize;
            file_inblocknumber = 0;
            while (sendsize >= FILE_BUF_SIZE)
            {
                memcpy(sendinfo.filebuffer, fileptr + sendinfo.filerealsize - sendsize, FILE_BUF_SIZE);
                sendinfo.filedatalength = FILE_BUF_SIZE;
                sendinfo.file_inblocknumber = file_inblocknumber++;
                // 向客户端发送数据
                send(fd, &sendinfo, sizeof(sendinfo), 0);
                // 更新剩余大小
                sendsize -= FILE_BUF_SIZE;
                printf("sending.\n");
            }
            // 发送最后一个数据包
            sendinfo.filedatalength = sendsize;
            sendinfo.file_inblocknumber = file_inblocknumber++;
            memcpy(sendinfo.filebuffer, fileptr + sendinfo.filerealsize - sendsize, sendsize);
            send(fd, &sendinfo, sizeof(sendinfo), 0);
            printf("%s数据发送完毕...\n", filepath);

            // 文件发送完毕
            close(filefd);
            munmap(fileptr, filesize);

            printf("数据发送完毕...\n");
            int i;
            for (i = 0; i < filetotalnumber; i++)
            {
                wait(NULL);
            }
            printf("子进程资源回收完毕...\n");
        }
        else
        { // 子进程
            snprintf(filepath, BUF_SIZE, "./usrsuploadfiles/%s0%d", myinfo->filename, i);
            printf("%s\n", filepath); // test
            filefd = open(filepath, O_RDWR);
            if (filefd == -1)
            {
                fprintf(stderr, "file open failed.\n");
                // 错误处理...
            }

            // 填充数据包
            struct fileinfo sendinfo;
            sendinfo.cmd = FILE_DOWNLOAD;
            sendinfo.filesize = filesize;
            sendinfo.filetotalnumber = filetotalnumber;
            sendinfo.fileorder = i;
            sendinfo.filerealsize = FILEBLOCK_MAXSIZE;

            // 映射文件到内存上
            fileptr = mmap(NULL, sendinfo.filerealsize, PROT_READ | PROT_WRITE, MAP_SHARED, filefd, 0);
            if (fileptr == (void *)-1)
            {
                fprintf(stderr, "mmap file failed.\n");
                // 错误处理...
            }

            // 开始发送
            sendsize = sendinfo.filerealsize;
            file_inblocknumber = 0;
            while (sendsize >= FILE_BUF_SIZE)
            {
                memcpy(sendinfo.filebuffer, fileptr + sendinfo.filerealsize - sendsize, FILE_BUF_SIZE);
                sendinfo.filedatalength = FILE_BUF_SIZE;
                sendinfo.file_inblocknumber = file_inblocknumber++;
                // 向客户端发送数据
                send(fd, &sendinfo, sizeof(sendinfo), 0);
                // 更新剩余大小
                sendsize -= FILE_BUF_SIZE;
                printf("sending.\n");
            }
            // 发送最后一个数据包
            sendinfo.filedatalength = sendsize;
            sendinfo.file_inblocknumber = file_inblocknumber++;
            memcpy(sendinfo.filebuffer, fileptr + sendinfo.filerealsize - sendsize, sendsize);
            send(fd, &sendinfo, sizeof(sendinfo), 0);
            printf("%s数据发送完毕...\n", filepath);

            // 文件发送完毕
            close(filefd);
            munmap(fileptr, filesize);
        }
    }
}