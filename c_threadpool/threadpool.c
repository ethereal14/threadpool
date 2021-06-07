#include "threadpool.h"

#define MAX_THREADS 32
#define MAX_QUEUE 256

static void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    for (;;)
    {
        /* 取得互斥资源 */
        pthread_mutex_lock(&(pool->lock));
        //使用while是为了在唤醒时重新检查条件
        while ((pool->count == 0) && (!pool->shutdown))
        {
            //任务队列为空，且线程池没有关闭时，阻塞在这里。
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        //判断线程池关闭方式，immediate_shutdown时，立即break，并退出
        //graceful_shutdown时，要等待线程池中任务被全部取走，即count == 0时
        if ((pool->shutdown == immediate_shutdown) || ((pool->shutdown == graceful_shutdown) && (pool->count == 0)))
        {
            break;
        }

        //取得任务队列第一个任务
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;

        //更新head、cout
        pool->head += 1;
        pool->head = (pool->head == pool->queue_size) ? 0 : pool->head;
        pool->count -= 1;

        //释放互斥资源
        pthread_mutex_unlock(&(pool->lock));

        //运行任务
        (*(task.function))(task.argument);
    }

    //线程将结束、更新运行线程数
    pool->started--;

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return NULL;
}

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{
    //对传入参数进行判断，MAX_THREADS、MAX_QUEUE为宏定义，自行定义
    if (thread_count <= 0 || thread_count > MAX_THREADS ||
        queue_size <= 0 || queue_size > MAX_QUEUE)
    {
        return NULL;
    }

    //声明线程池变量
    threadpool_t *pool;

    //动态申请线程池空间，失败goto err
    if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
    {
        goto err;
    }

    //对线程池结构体参数进行初始化
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    //线程数组动态申请
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    //任务队列动态申请
    pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);

    //初始化互斥锁、条件变量并判断线程数组、任务队列等是否申请(初始化成功)
    if ((pthread_mutex_init(&pool->lock, NULL) != 0) ||
        (pthread_cond_init(&pool->notify, NULL) != 0) ||
        (pool->threads == NULL) || (pool->queue == NULL))
    {
        goto err;
    }

    //创建指定的线程并开始运行
    for (size_t i = 0; i < thread_count; i++)
    {
        if (pthread_create(&pool->threads[i], NULL, threadpool_thread, (void *)pool) != 0)
        {
            threadpool_destroy(pool, 0);
            return NULL;
        }
        pool->thread_count++; //每成功创建一个线程，工作线程数量++
        pool->started++;      //每运行一个线程，运行中线程数量++
    }
    return pool;

err:
    if (pool)
    {
        threadpool_free(pool);
    }
    return NULL;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument, int flags)
{
    int err = 0;
    int next;
    //参数判断
    if (pool == NULL || function == NULL)
    {
        return threadpool_invalid;
    }
    //对资源进行上锁。若上锁失败，会阻塞。
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return threadpool_lock_failure;
    }

    //计算下一个可以存储任务的位置
    next = pool->tail + 1;
    //判断next到没到任务队列的上限，没有就next = next
    next = (next == pool->queue_size) ? 0 : next;

    //此处使用do {...}while(0)结构是为了保证至少运行一次。如果有异常，直接break掉，不运行后边的语句
    do
    {
        //判断线程池中任务数量是否小于任务队列上限
        if (pool->count == pool->queue_size)
        {
            err = threadpool_queue_full;
            break;
        }

        //添加任务到任务队列。queue实际大小是0~255，而next是从1开始，1~256
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;

        //这句就相当于pool->tail++
        pool->tail = next;
        //任务数量++
        pool->count += 1;

        //条件变量。任务添加到队列以后，发出信号通知空闲线程取任务。
        if (pthread_cond_signal(&(pool->notify)) != 0)
        {
            err = threadpool_lock_failure;
            break;
        }
    } while (0);

    //释放资源
    if (pthread_mutex_unlock(&(pool->lock)) != 0)
    {
        err = threadpool_lock_failure;
    }

    return err;
}

int threadpool_destroy(threadpool_t *pool, int flags)
{
    int i, err = 0;

    if (pool == NULL)
        return threadpool_invalid;
    //对资源进行上锁(取得互斥锁资源)
    if (pthread_mutex_lock(&(pool->lock)) != 0)
        return threadpool_lock_failure;

    //此处使用do {...}while(0)结构是为了保证至少运行一次。如果有异常，直接break掉，不运行后边的语句
    do
    {
        //判断是否已经关闭了线程池
        if (pool->shutdown)
        {
            err = threadpool_shutdown;
            break;
        }

        //指定线程池关闭方式;graceful_shutdown为自动关闭，即等待所有线程完成任务后退出
        //immediate_shutdown，立即退出，不等待
        pool->shutdown = (flags & threadpool_graceful) ? graceful_shutdown : immediate_shutdown;

        //唤醒所有因条件变脸阻塞的线程，并释放互斥锁
        if ((pthread_cond_broadcast(&(pool->notify)) != 0) || (pthread_mutex_unlock(&(pool->lock)) != 0))
        {
            err = threadpool_lock_failure;
            break;
        }

        //等待子线程退出、并回收线程资源
        for (size_t i = 0; i < pool->thread_count; i++)
        {
            if (pthread_join(pool->threads[i], NULL) != 0)
            {
                err = threadpool_thread_failure;
            }
        }
    } while (0);

    if (!err)
    {
        threadpool_free(pool);
    }

    return err;
}

int threadpool_free(threadpool_t *pool)
{
    //判断参数是否合法
    if (pool == NULL || pool->started > 0)
    {
        return -1;
    }

    //释放互斥锁、条件变量、malloc出的条件队列、线程数组等
    if (pool->threads)
    {
        free(pool->threads);
        free(pool->queue);

        //以防万一，加锁后再destroy。不写应该没有啥问题
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);
    return 0;
}
