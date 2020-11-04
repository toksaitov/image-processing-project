#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "work_item.h"
#include "sync_queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Useful Helpers */

#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <sys/param.h>
    #include <sys/sysctl.h>
#else
    #include <unistd.h>
#endif

static size_t utils_get_number_of_cpu_cores()
{
    int result;

#ifdef WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    result =
        (int) info.dwNumberOfProcessors;
#elif __APPLE__
    int nm[2];

    size_t length = 4;
    uint32_t count;

    nm[0] = CTL_HW;
    nm[1] = HW_AVAILCPU;

    sysctl(nm, 2, &count, &length, NULL, 0);

    if (count < 1) {
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &length, NULL, 0);
    }

    result =
        (int) count;
#else
    result =
        (int) sysconf(_SC_NPROCESSORS_ONLN);
#endif

    if (result < 1) {
        result = 1;
    }

    return (size_t) result;
}

/* Threadpool */

typedef struct _threadpool
{
    sync_queue_t *queue;

    pthread_t *threads;
    size_t thread_count;
    unsigned long request_to_terminate;
} threadpool_t;

static inline bool check_if_signaled(unsigned int event)
{
    return event > 0;
}

static void *_thread_start(void *args)
{
    threadpool_t *threadpool = (threadpool_t *) args;
    sync_queue_t *queue = threadpool->queue;
    while (true) {
        if(check_if_signaled(threadpool->request_to_terminate))
        {
            break;
        }
        work_item_t *work_item = (work_item_t *) sync_queue_pop(queue);
        if (NULL == work_item) {
            continue;
        }

        work_item->task(work_item->task_data, work_item->result_callback);
        work_item_destroy(work_item);
    }

    return NULL;
}

static inline threadpool_t *threadpool_allocate(void)
{
    return (threadpool_t *) malloc(sizeof(threadpool_t));
}

static threadpool_t *threadpool_init(threadpool_t *threadpool, size_t pool_size)
{
    threadpool->request_to_terminate = 0;
    threadpool->thread_count =
        pool_size;

    threadpool->queue = sync_queue_create();
    if (NULL == threadpool->queue) {
        return NULL;
    }

    threadpool->threads = (pthread_t *) malloc(sizeof(pthread_t) * pool_size);
    if (NULL == threadpool->threads) {
        sync_queue_destroy(threadpool->queue);
        threadpool->queue = NULL;

        return NULL;
    }

    for (size_t i = 0; i < pool_size; ++i) {
        pthread_create(
            &threadpool->threads[i],
            NULL,
            _thread_start,
            (void *) threadpool
        );
    }

    return threadpool;
}

static inline threadpool_t *threadpool_create(size_t pool_size)
{
    threadpool_t *threadpool = threadpool_allocate();
    if (NULL == threadpool) {
        return threadpool;
    }

    if (NULL == threadpool_init(threadpool, pool_size)) {
        free(threadpool);
        threadpool = NULL;

        return NULL;
    }

    return threadpool;
}

static void threadpool_destroy(threadpool_t *threadpool)
{
    __sync_add_and_fetch(&threadpool->request_to_terminate, 1);
    if (NULL == threadpool) {
        return;
    }

    if (NULL != threadpool->threads) {
        for (size_t i = 0; i < threadpool->thread_count; ++i) {
            pthread_join(threadpool->threads[i], NULL);
        }

        free(threadpool->threads);
        threadpool->threads = NULL;
    }

    if (NULL != threadpool->queue) {
        sync_queue_destroy(threadpool->queue);
        threadpool->queue = NULL;
    }

    free(threadpool);
    threadpool = NULL;
}

static inline void threadpool_enqueue_task(
                       threadpool_t *threadpool,
                       void (*task)(void *task_data, void (*result_callback)(void *result)),
                       void *task_data,
                       void (*result_callback)(void *result)
                   )
{
    work_item_t *work_item = work_item_create(task, task_data, result_callback);
    if (NULL == work_item) {
        return;
    }

    sync_queue_enqueue(threadpool->queue, work_item);
}

#endif // THREADPOOL_H
