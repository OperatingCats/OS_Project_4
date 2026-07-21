#include "node_queue.h"
#include "errors.h"
#include <stdlib.h>
#include <string.h>

int node_queue_init(node_queue_t *q) {
    if (q == NULL) {
        return ERR_INVALID_ARGUMENT;
    }
    memset(q, 0, sizeof(*q));
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutting_down = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return PROJECT_OK;
}

void node_queue_destroy(node_queue_t *q) {
    if (q == NULL) {
        return;
    }
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

int node_queue_push(node_queue_t *q, const queue_message_t *msg) {
    if (q == NULL || msg == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count == NODE_QUEUE_CAPACITY && !q->shutting_down) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    if (q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        return ERR_IPC;
    }

    q->items[q->tail] = *msg;
    q->tail = (q->tail + 1) % NODE_QUEUE_CAPACITY;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return PROJECT_OK;
}

int node_queue_pop(node_queue_t *q, queue_message_t *out) {
    if (q == NULL || out == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->shutting_down) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->count == 0 && q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        return ERR_NOT_FOUND;
    }

    *out = q->items[q->head];
    q->head = (q->head + 1) % NODE_QUEUE_CAPACITY;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return PROJECT_OK;
}

void node_queue_shutdown(node_queue_t *q) {
    if (q == NULL) {
        return;
    }
    pthread_mutex_lock(&q->lock);
    q->shutting_down = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}
