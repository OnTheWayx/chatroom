#ifndef _USERLINKLIST_H_
#define _USERLINKLIST_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "userinfo.h"

// 链表结点
typedef struct userlinklist
{
    int fd;

    // 心跳计数
    int heartcount;

    // 用户信息
    usrinfo user;
    // 用户打开的文件指针
    FILE *file;
    // 用户数据传送锁
    pthread_mutex_t mutex;

    struct userlinklist *next;
}userlinklist;

// 创建带头结点的链表
// 返回指向头结点的指针
userlinklist *CreateList();

// 销毁带头结点的链表（包括头结点）
void DestroyList(userlinklist **head);

// 插入结点到链表
int InsertList(userlinklist *head, int fd, usrinfo info);

// 从链表中删除结点
int EraseList(userlinklist *head, int fd);

// 从链表获取目标结点
userlinklist *GetNode_fd(userlinklist *head, int fd);
userlinklist *GetNode_id(userlinklist *head, const char *id);

#endif // end of userlinklist.h