#include "../serverHeaders/userlinklist.h"

userlinklist *CreateList()
{
    userlinklist *head = (userlinklist *)malloc(sizeof(userlinklist));
    if (head == NULL)
    {
        return NULL;
    }
    head->next = NULL;

    return head;
}

void DestroyList(userlinklist **head)
{
    if (head == NULL || *head == NULL)
    {
        return;
    }
    // 指向头结点
    userlinklist *p = (*head), *tmp;

    while (p->next != NULL)
    {
        tmp = p->next;
        p->next = tmp->next;

        free(tmp);
        tmp = NULL;
    }

    free(*head);
    *head = NULL;

    return;
}

int InsertList(userlinklist *head, int fd, usrinfo info)
{
    userlinklist *new = (userlinklist *)malloc(sizeof(userlinklist));
    if (new == NULL)
    {
        return -1;
    }
    memset(new, sizeof(userlinklist), 0);
    new->fd = fd;
    strcpy_usrinfo(&(new->user), &info);
    // 初始化文件指针
    new->file = NULL;
    // 初始化锁
    pthread_mutex_init(&(new->mutex), NULL);
    new->next = NULL;

    new->next = head->next;
    head->next = new;

    return 0;
}

int EraseList(userlinklist *head, int fd)
{
    userlinklist *tmp = head, *del;

    while (tmp->next != NULL)
    {
        if (tmp->next->fd == fd)
        {
            del = tmp->next;
            tmp->next = del->next;

            free(del);
            del = NULL;

            return 0;
        }
        tmp = tmp->next;
    }

    return -1;
}

userlinklist *GetNode_fd(userlinklist *head, int fd)
{
    userlinklist *tmp = head;

    while (tmp->next != NULL)
    {
        
        if (tmp->next->fd == fd)
        {
            return tmp->next;
        }
        tmp = tmp->next;
    }

    return NULL;
}

userlinklist *GetNode_id(userlinklist *head, const char *id)
{
    userlinklist *tmp = head;

    while (tmp->next != NULL)
    {
        if (strncmp(tmp->next->user.id, id, 6) == 0)
        {
            return tmp->next;
        }
        tmp = tmp->next;
    }

    return NULL;
}