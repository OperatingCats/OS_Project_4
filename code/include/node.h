#ifndef NODE_H
#define NODE_H

#include <pthread.h>
#include "blockchain.h"
#include "node_queue.h"
#include "logger.h"
#define NODE_WORKER_COUNT 4

typedef struct {
    int node_id;
    int is_coordinator;
    char runtime_dir[256];
    char sock_path[MAX_SOCKET_PATH_LEN];

    int server_fd;
    FILE *log_file;

    blockchain_t chain;
    pthread_mutex_t chain_lock;

    node_queue_t queue;
    pthread_t receiver_thread;
    pthread_t workers[NODE_WORKER_COUNT];

    volatile int running;
} node_t;

int node_init(node_t *node, int node_id, const char *runtime_dir);
void node_destroy(node_t *node);
int node_run(node_t *node);
void node_stop(node_t *node);

#endif /* NODE_H */
