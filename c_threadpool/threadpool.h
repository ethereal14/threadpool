#ifndef __THREAD_POOL_

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    void (*function)(void *); //函数指针，参数为void*。任意线程函数
    void *argument;           //线程函数参数
} threadpool_task_t;

typedef struct threadpool_t
{
    pthread_mutex_t lock;     //用于内部工作的互斥锁
    pthread_cond_t notify;    //线程间通知的条件变量
    pthread_t *threads;       //线程数组
    threadpool_task_t *queue; //存储任务的数组,即任务队列
    int thread_count;         //线程数量
    int queue_size;           //任务队列大小
    int head;                 //任务队列中首个任务位置（注：任务队列中所有任务都是未开始运行的）
    int tail;                 //任务队列中最后一个任务的下一个位置（注：队列以数组存储，head 和 tail 指示队列位置）
    int count;                //任务队列里的任务数量，即等待运行的任务数
    int shutdown;             //表示线程池是否关闭
    int started;              //开始的线程数
} threadpool_t;

typedef enum
{
    threadpool_invalid = -1,
    threadpool_lock_failure = -2,
    threadpool_queue_full = -3,
    threadpool_shutdown = -4,
    threadpool_thread_failure = -5
} threadpool_terror_t;

typedef enum
{
    immediate_shutdown = 1, //自动关闭
    graceful_shutdown = 2   //立即关闭
} threadpool_shutdown_t;

typedef enum
{
    threadpool_graceful = 1
} threadpool_destroy_flags_t;

/**
 * @function 			threadpool_create
 * @brief	 			创建线程池
 * @param thread_count 	工作线程数量
 * @param thread_queue 	任务队列大小
 * @param flags 	   	未使用的参数
 * @return 				返回一个新创建的线程池或者NULL
**/
threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);

/**
 * @function 			threadpool_add
 * @brief	 			添加新任务到线程池队列中
 * @param pool 			线程池指针
 * @param routine 		函数指针
 * @param arg       	传入函数的参数
 * @param flags 		未使用的参数
 * @return 				成功返回0，失败返回错误码
**/
int threadpool_add(threadpool_t *pool, void (*routine)(void *), void *arg, int flags);

/**
 * @function 			threadpool_destroy
 * @brief	 			停止并且摧毁线程池
 * @param pool 			线程池指针
 * @param flags 		指定关闭方式
**/
int threadpool_destroy(threadpool_t *pool, int flags);

static void *threadpool_thread(void *threadpool);

int threadpool_free(threadpool_t *pool);

#endif // !__THREAD_POOL_