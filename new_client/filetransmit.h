#ifndef _FILETRANSMIT_H_
#define _FILETRANSMIT_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>

#include "client.h"
#include "protocol.h"
#include "fileclientoption.h"

#define BUF_SIZE 1024
#define SUCCESS 0
#define FAILURE -1

void DownloadFile(client_t *client, const char *filename, long filesize);

/**
 * // 建立传输文件的连接
 * 成功返回0,失败返回-1
 */ 
int Connect(client_t *client);

#endif // end of filetransmit.h