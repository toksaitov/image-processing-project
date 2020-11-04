#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include "queue.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

typedef struct _sync_queue
{
    pthread_mutex_t access_mutex;
    pthread_cond_t not_empty_condition;
    queue_t implementation;
} sync_queue_t;

static inline sync_queue_t *sync_queue_allocate()
{
    return (sync_queue_t *) malloc(sizeof(sync_queue_t));
}

static inline sync_queue_t *sync_queue_init(sync_queue_t *queue)
{
    if (0 != pthread_mutex_init(&queue->access_mutex, NULL)) {
        return NULL;
    }

    if (0 != pthread_cond_init(&queue->not_empty_condition, NULL)) {
        pthread_mutex_destroy(&queue->access_mutex);

        return NULL;
    }
    queue_init(&queue->implementation);

    return queue;
}

static inline sync_queue_t *sync_queue_create()
{
    sync_queue_t *queue = sync_queue_allocate();
    if (NULL == queue) {
        return queue;
    }

    if (NULL == sync_queue_init(queue)) {
        free(queue);
        queue = NULL;

        return NULL;
    }

    return queue;
}

static inline void sync_queue_destroy(sync_queue_t *queue)
{
    if (NULL == queue) {
        return;
    }

    pthread_mutex_destroy(&queue->access_mutex);
    pthread_cond_destroy(&queue->not_empty_condition);
    //Queue is allocated on stack, not heap; Thus queue_destroy is replaced;
    queue_deinit(&queue->implementation);
    free(queue);
    queue = NULL;
}

static inline size_t sync_queue_get_size(sync_queue_t *queue)
{
    return (size_t) queue_get_size(&queue->implementation);
}

static inline bool sync_queue_is_empty(sync_queue_t *queue)
{
    return queue_is_empty(&queue->implementation);
}

static sync_queue_t *sync_queue_enqueue(sync_queue_t *queue, void *data)
{
    if (0 != pthread_mutex_lock(&queue->access_mutex)) {
        return NULL;
    }

    queue_push(&queue->implementation, data);
    pthread_cond_broadcast(&queue->not_empty_condition);

    if (0 != pthread_mutex_unlock(&queue->access_mutex)) {
        return NULL;
    }

    return queue;
}

static void *sync_queue_pop(sync_queue_t *queue)
{
    void *data = NULL;

    if (0 != pthread_mutex_lock(&queue->access_mutex)) {
        return data;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;

    while (queue_is_empty(&queue->implementation)) {
        int res = pthread_cond_timedwait(&queue->not_empty_condition, &queue->access_mutex, &ts);
        if (0 != res && res != ETIMEDOUT) {
            return data;
        }
        else if(res == ETIMEDOUT)
        {
            break;
        }
    }

    data = queue_pop(&queue->implementation);

    if (0 != pthread_mutex_unlock(&queue->access_mutex)) {
        return data;
    }

    return data;
}

#endif // SYNC_QUEUE_H
