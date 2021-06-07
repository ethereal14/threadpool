#include "threadpool.h"

#define MAX_THREADS 32
#define MAX_QUEUE 256

void *threadpool::threadpool_thread(void *arg)
{
    threadpool *pool = static_cast<threadpool *>(arg);
    threadpool_task task;

    for (;;)
    {
        pthread_mutex_lock(&(pool->m_lock));
        while ((pool->m_count == 0) && (!pool->m_shutdown))
        {
            pthread_cond_wait(&(pool->m_notify), &(pool->m_lock));
        }
        if ((pool->m_shutdown == immediate_shutdown) || ((pool->m_shutdown == graceful_shutdown) && (pool->m_count == 0)))
        {
            break;
        }

        task.function = pool->m_queue[pool->m_head].function;
        task.argument = pool->m_queue[pool->m_head].argument;

        pool->m_head++;
        pool->m_head = (pool->m_head == pool->m_queue_size) ? 0 : pool->m_head;
        pool->m_count--;

        pthread_mutex_unlock(&(pool->m_lock));

        // (*(task.function))(task.argument);

        task.function(task.argument);
    }

    pool->m_started--;
    pthread_mutex_unlock(&(pool->m_lock));
    pthread_exit(nullptr);
    return nullptr;
}

threadpool_task::threadpool_task()
{
}

threadpool_task::~threadpool_task()
{
}

threadpool::threadpool(int thread_count = 32, int queue_size = 256, int flags = 0) : m_queue_size(queue_size)
{
    do
    {
        if (thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
        {
            std::cout << "argument eeror" << std::endl;
            break;
        }

        this->m_thread_count = 0;
        //this->m_queue_size = queue_size;
        this->m_head = this->m_tail = this->m_count = 0;
        this->m_shutdown = this->m_started = 0;

        this->m_threads = new pthread_t[thread_count];
        this->m_queue = new threadpool_task[queue_size];

        if ((pthread_mutex_init(&this->m_lock, nullptr) != 0) ||
            (pthread_cond_init(&this->m_notify, NULL) != 0) ||
            (this->m_threads == nullptr) || (this->m_queue == nullptr))
        {
            std::cout << "init eeror" << std::endl;
            break;
        }

        for (size_t i = 0; i < thread_count; i++)
        {
            if (pthread_create(&this->m_threads[i], nullptr, threadpool_thread, this) != 0)
            {
                std::cout << "create " << i << " thread error" << std::endl;
                break;
            }
            this->m_thread_count++;
            this->m_started++;
        }

    } while (0);
}

int threadpool::threadpoolexit(threadpool *pool, int flag)
{
    int i, err = 0;

    if (pool == nullptr)
        return threadpool_invalid;

    if (pthread_mutex_lock(&(pool->m_lock)) != 0)
        return threadpool_lock_failure;

    do
    {
        if (pool->m_shutdown)
        {
            err = threadpool_shutdown;
            break;
        }
        pool->m_shutdown = (flag & threadpool_graceful) ? graceful_shutdown : immediate_shutdown;

        if ((pthread_cond_broadcast(&(pool->m_notify)) != 0) || (pthread_mutex_unlock(&(pool->m_lock)) != 0))
        {
            err = threadpool_lock_failure;
            break;
        }

        for (size_t i = 0; i < pool->m_thread_count; i++)
        {
            if (pthread_join(pool->m_threads[i], nullptr) != 0)
                err = threadpool_thread_failure;
        }

    } while (0);

    return err;
}

int threadpool::threadpool_add(threadpool *pool, void (*function)(void *), void *argument, int flag)
{
    int err = 0, next;

    if (pool == nullptr || function == nullptr)
        return threadpool_invalid;
    if (pthread_mutex_lock(&(pool->m_lock)) != 0)
    {
        return threadpool_lock_failure;
    }

    next = pool->m_tail + 1;

    next = (next == pool->m_queue_size) ? 0 : next;

    do
    {
        if (pool->m_count == pool->m_queue_size)
        {
            err = threadpool_queue_full;
            break;
        }
        pool->m_queue[pool->m_tail].function = function;
        pool->m_queue[pool->m_tail].argument = argument;

        pool->m_tail = next;
        pool->m_count++;

        if (pthread_cond_signal(&(pool->m_notify)) != 0)
        {
            err = threadpool_lock_failure;
            break;
        }

    } while (0);

    if (pthread_mutex_unlock(&(pool->m_lock)) != 0)
    {
        err = threadpool_lock_failure;
    }

    return err;
}

threadpool::~threadpool()
{
    if (this == NULL || this->m_started > 0)
    {
        exit(-1);
    }
    if (this->m_threads)
    {
        delete[] this->m_threads;
        delete[] this->m_queue;

        pthread_mutex_lock(&(this->m_lock));
        pthread_mutex_destroy(&(this->m_lock));
        pthread_cond_destroy(&(this->m_notify));
    }
    //delete[] this;
}