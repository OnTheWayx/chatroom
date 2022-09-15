#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

// 文件传输协议
// 文件传输块有效数据最大长度
#define FILE_BUF_SIZE (1024 * 3)
// 客户端请求信息结构体大小 
#define INFOSIZE 1024

// 单个传输文件块最大为512M
#define FILEBLOCK_MAXSIZE (512 * 1024 * 1024)
#define FILEBLOCK_MAXSIZE_MB 512

#define FILENAMESIZE 60

// 客户端请求信息结构体
// 固定1024字节
typedef struct file_normalinfo
{
    // 命令
    int cmd;
    // 填充到结构体为1024字节
    unsigned char _fill[INFOSIZE - sizeof(int)];
}file_normalinfo;

// 固定1024字节
typedef struct uploadfile_info
{
    // 命令
    int cmd;

    // 文件名
    char filename[FILENAMESIZE];

    // 填充到结构体为1024字节
    unsigned char _fille[INFOSIZE - sizeof(int) - sizeof(char) * FILENAMESIZE];
}uploadfile_info;

typedef struct fileinfo
{
    // 命令
    int cmd;                 
    // 本块有效数据大小
    int filerealsize;

    // 文件总大小
    long filesize;

    // 当前数据包有效数据
    int filedatalength;

    // 文件分块总数
    int filetotalnumber;

    // 本文件当前块号,从0开始
    int fileorder;

    // 当前数据包的块内编号
    int file_inblocknumber;

    // 文件数据缓冲区3k
    char filebuffer[FILE_BUF_SIZE];
}fileinfo;

#endif // end of protocol.h