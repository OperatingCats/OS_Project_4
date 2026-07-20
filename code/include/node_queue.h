#ifndef NODE_QUEUE_H
#define NODE_QUEUE_H

#include <pthread.h>
#include "protocol.h"

/* A protected FIFO queue between the Node's connection-accepting thread
 * and its worker thread pool. Each entry represents one already-received
 * IPC message (header + heap payload from ipc_recv) plus the client fd
 * a worker should reply on and close when done. */

#define NODE_QUEUE_CAPACITY 64

typedef struct {
    message_header_t header;
    void *payload;     /* owned by the queue entry; worker frees after use */
    int client_fd;      /* worker replies via ipc_send then ipc_close */
} queue_message_t;

typedef struct {
    queue_message_t items[NODE_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    int shutting_down;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} node_queue_t;

/* Returns PROJECT_OK on success. */
int node_queue_init(node_queue_t *q);
void node_queue_destroy(node_queue_t *q);

/* Blocks if the queue is full. Returns ERR_INVALID_ARGUMENT if q/msg are
 * NULL, PROJECT_OK on success. */
int node_queue_push(node_queue_t *q, const queue_message_t *msg);

/* Blocks if the queue is empty. Returns ERR_NOT_FOUND if the queue was
 * shut down and is empty (signals the worker to exit), PROJECT_OK
 * otherwise with *out filled in. */
int node_queue_pop(node_queue_t *q, queue_message_t *out);

/* Wakes all blocked threads so they can notice shutdown and exit. */
void node_queue_shutdown(node_queue_t *q);

#endif /* NODE_QUEUE_H */
