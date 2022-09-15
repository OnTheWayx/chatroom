#include "threadpool.h"

threadpool *threadpool_create(const int thread_max_num, const int queue_max_size)
{
    int i;

    threadpool *pool = NULL;

    do
    {
        //线程池开空间
        pool = (threadpool *)malloc(sizeof(threadpool));
        if (NULL == pool)
        {
            printf("线程池申请空间失败\n");
            break;
        }
        //线程id号数组开空间
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_max_num);
        if (NULL == pool->threads)
        {
            printf("线程id数组申请空间失败\n");
            break;
        }
        //清空线程id号数组所指内存空间的内容
        memset(pool->threads, 0, sizeof(pthread_t) * thread_max_num);
        //任务队列开空间
        pool->task_queue = (task_t *)malloc(sizeof(task_t) * queue_max_size);
        if (NULL == pool->task_queue)
        {
            printf("任务队列申请空间失败\n");
            break;
        }
        //初始化线程池参数
        pool->thread_max_num = thread_max_num;
        pool->queue_max_size = queue_max_size;
        pool->queue_front = 0;
        pool->queue_rear = 0;
        pool->queue_size = 0;
        pool->thread_busy_num = 0;
        pool->shutdown = false;

        //初始化互斥锁和条件变量
        if (0 != pthread_mutex_init(&(pool->pool_mutex), NULL) || 0 != pthread_mutex_init(&(pool->thread_busy_mutex), NULL) || 0 != pthread_cond_init(&(pool->queue_not_empty), NULL) || 0 != pthread_cond_init(&(pool->queue_not_full), NULL))
        {
            printf("互斥锁或条件变量申请空间失败\n");
            break;
        }
        //创建相应数量的线程
        for (i = 0; i < thread_max_num; i++)
        {
            if (0 != pthread_create(&(pool->threads[i]), NULL, thread_function, pool))
            {
                printf("线程创建失败\n");
                break;
            }
        }

        return pool;
    } while (0);
    threadpool_free(pool);

    return NULL;
}

void *thread_function(void *arg)
{
    threadpool *pool = (threadpool *)arg;

    task_t task;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    while (1)
    {
        pthread_mutex_lock(&(pool->pool_mutex));
        //若任务队列为空且线程池处于开启状态，则阻塞等待任务的到来
        while ((0 == pool->queue_size) && (!pool->shutdown))
        {
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->pool_mutex));
        }
        //若线程池处于关闭状态
        //则线程在取任务的途中结束自身
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->pool_mutex));
            pthread_exit(NULL);
        }

        //从任务队列中取任务
        task.function = pool->task_queue[pool->queue_front].function;
        task.recvfd = pool->task_queue[pool->queue_front].recvfd;
        task.recvinfo = pool->task_queue[pool->queue_front].recvinfo;
        //任务队列对头向后移一个位置（环形队列）
        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
        //任务队列任务数减一
        pool->queue_size--;
        //若任务队列满而导致添加任务的函数所在线程阻塞，则唤醒被阻塞线程
        pthread_cond_broadcast(&(pool->queue_not_full));
        //线程池解锁
        pthread_mutex_unlock(&(pool->pool_mutex));

        //开始执行任务
        //忙线程数加一
        pthread_mutex_lock(&(pool->thread_busy_mutex));
        pool->thread_busy_num++;
        pthread_mutex_unlock(&(pool->thread_busy_mutex));
        (*(task.function))(task.recvfd, task.recvinfo);
        //任务执行完毕，忙线程数减一
        pthread_mutex_lock(&(pool->thread_busy_mutex));
        pool->thread_busy_num--;
        pthread_mutex_unlock(&(pool->thread_busy_mutex));
    }
}

int thread_add_task(threadpool *pool, void (*function)(const int, const void *), int recvfd, void *recvinfo)
{
    if (NULL == pool)
    {
        return -1;
    }
    //线程池上锁
    pthread_mutex_lock(&(pool->pool_mutex));
    //若任务队列满且线程池处于开启状态，则阻塞等待其他线程处理万任务腾出任务队列空间
    while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
    {
        printf("任务队列满.\n");
        pthread_cond_wait(&(pool->queue_not_full), &(pool->pool_mutex));
    }
    //若线程池处于关闭状态，则不可向其中添加任务，直接返回-1
    if (pool->shutdown)
    {
        //线程池解锁
        pthread_mutex_unlock(&(pool->pool_mutex));

        return -1;
    }

    //将任务添加到任务队列队尾
    pool->task_queue[pool->queue_rear].function = function;
    pool->task_queue[pool->queue_rear].recvfd = recvfd;
    pool->task_queue[pool->queue_rear].recvinfo = recvinfo;
    //队尾向后移动一个位置（环形队列）
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;
    //任务队列任务数量加一
    pool->queue_size++;
    //若有线程处于睡眠状态等待任务的到来，则唤醒线程处理任务
    pthread_cond_signal(&(pool->queue_not_empty));
    //线程池解锁
    pthread_mutex_unlock(&(pool->pool_mutex));

    return 0;
}

int threadpool_free(threadpool *pool)
{
    if (NULL == pool)
    {
        return -1;
    }
    if (pool->threads)
    {
        //释放线程id数组的空间
        //销毁互斥锁和条件变量
        free(pool->threads);
        pthread_mutex_trylock(&(pool->pool_mutex));
        pthread_mutex_destroy(&(pool->pool_mutex));
        pthread_mutex_trylock(&(pool->thread_busy_mutex));
        pthread_mutex_destroy(&(pool->thread_busy_mutex));
        pthread_cond_destroy(&(pool->queue_not_empty));
        pthread_cond_destroy(&(pool->queue_not_full));
    }
    if (pool->task_queue)
    {
        //释放任务队列空间
        free(pool->task_queue);
    }
    //释放线程池空间
    free(pool);

    return 0;
}

int threadpool_destroy(threadpool *pool)
{
    if (NULL == pool)
    {
        return -1;
    }

    int i;

    //将线程池设为关闭状态
    pool->shutdown = true;
    //唤醒阻塞的线程（在取任务的过程中结束自己）
    pthread_cond_broadcast(&(pool->queue_not_empty));
    //回收线程资源
    for (i = 0; i < pool->thread_max_num; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
    threadpool_free(pool);
    printf("线程资源都已回收\n");
    return 0;
}