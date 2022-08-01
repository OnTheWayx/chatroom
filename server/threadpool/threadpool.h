#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define true 1
#define false 0

//任务队列结点
typedef struct 
{
    void (*function)(const int, const void *);
    int recvfd;
    void *recvinfo;
}task_t;

//线程池结构体
typedef struct
{
    //线程id数组
    pthread_t *threads;
    //忙线程的数量
    int thread_busy_num;
    //线程池中线程的数量
    int thread_max_num;

    //任务队列
    task_t *task_queue;
    //任务队列头
    int queue_front;
    //任务队列尾
    int queue_rear;
    //任务队列中任务数量
    int queue_size;
    //任务队列能容纳的最大任务数量
    int queue_max_size;

    //修改线程池中参数的互斥锁
    pthread_mutex_t pool_mutex;
    //修改忙线程数的互斥锁
    pthread_mutex_t thread_busy_mutex;
    //队列不为空条件变量，当任务队列为空而线程取任务时
    //会进入阻塞状态，等待有任务时被唤醒
    pthread_cond_t queue_not_empty;
    //任务队列不为满条件变量，当任务队列为满而要添加任务时
    //会进入阻塞状态，等待线程处理完任务唤醒添加任务的函数所在线程
    pthread_cond_t queue_not_full;

    //线程池状态（true时，线程池关闭并销毁，默认false）
    int shutdown;
}threadpool;

/**
 * @brief 
 * 创建线程池
 * 
 * @param thread_max_num 线程池中线程的数量 
 * @param queue_max_size 任务队列所能包含的最大任务数量
 * @return threadpool* 创建成功，返回创建的线程池的首地址，创建失败返回NULL
 */
threadpool *threadpool_create(const int thread_max_num, const int queue_max_size);
/**
 * 线程执行函数
 * 负责取任务，执行任务
 */
void *thread_function(void *arg);
/**
 * @brief 
 * 向任务队列中添加任务
 * 
 * @param pool 目标线程池
 * @param function 任务队列结点
 * @param recvfd 任务参数
 * @param recvinfo 任务参数
 * @return int 添加成功返回0，失败返回-1
 */
int thread_add_task(threadpool *pool, void (*function)(const int, const void *), int recvfd, void *recvinfo);
/**
 * @brief 
 * 释放目标线程池以及其中参数所申请的空间
 * 销毁目标线程池的互斥锁和条件变量
 * 
 * @param pool 目标线程池
 * @return int 成功返回0，失败返回-1
 */
int threadpool_free(threadpool *pool);
/**
 * @brief 
 * 销毁并回收目标线程池的线程
 * 其中调用了函数threadpool_free
 * 
 * @param pool 目标线程池
 * @return int 成功返回0，失败返回-1
 */
int threadpool_destroy(threadpool *pool);

#endif