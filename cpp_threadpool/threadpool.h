#ifndef _THREAD_POOL_
#define _THREAD_POOL_

#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <unistd.h>

class threadpool_task
{
public:
    void (*function)(void *);
    void *argument;
    threadpool_task();
    ~threadpool_task();
};

class threadpool
{
private:
    pthread_mutex_t m_lock;
    pthread_cond_t m_notify;
    pthread_t *m_threads;
    threadpool_task *m_queue;

    int m_thread_count;
    int m_queue_size;
    int m_head;
    int m_tail;
    int m_count;
    int m_shutdown;
    int m_started;

private:
    static void *threadpool_thread(void *arg);

public:
    threadpool(int thread_count, int queue_size, int flags);
    ~threadpool();

    int threadpool_add(threadpool *pool, void (*function)(void *), void *argument, int flag);
    int threadpoolexit(threadpool *pool, int flag);
};

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

#endif // !_THREAD_POOL_