#include "threadpool.h"
#include <iostream>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#define THREAD 32
#define QUEUE 256

int tasks = 0, done = 0;
pthread_mutex_t lock;

void dumy_task(void *arg)
{
    usleep(10000);
    pthread_mutex_lock(&lock);

    done++;
    pthread_mutex_unlock(&lock);
}

int main(int, char **)
{
    threadpool *pool = new threadpool(THREAD, QUEUE, 0);

    pthread_mutex_init(&lock, nullptr);

    std::cout << "pool started with " << THREAD << " threads and queue size of " << QUEUE << std::endl;

    while ((pool->threadpool_add(pool, &dumy_task, nullptr, 0)) == 0)
    {
        pthread_mutex_lock(&lock);
        tasks++;
        pthread_mutex_unlock(&lock);
    }
    std::cout << "add " << tasks << " tasks" << std::endl;

    while ((tasks / 2) > done)
    {
        usleep(10000);
    }
    //pool->threadpoolexit(pool, 0) == 0;
    assert(pool->threadpoolexit(pool, 0) == 0);
    std::cout << "did " << done << " tasks" << std::endl;

    delete[] pool;

    return 0;
}
