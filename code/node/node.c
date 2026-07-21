#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include "block.h"
#include "validation.h"
#include "blockchain.h"
#include "node.h"
#include "protocol.h"
#include "ipc.h"
#include "errors.h"
#include "node_queue.h"

static void *receiver_main(void *arg) {
    node_t *node = (node_t *)arg;

    while (node->running) {
        int client_fd;
        int rc = ipc_server_accept(node->server_fd, &client_fd);
        if (rc != PROJECT_OK) {
            if (node->running) {
                fprintf(stderr, "node %d: accept failed (err %d)\n",
                        node->node_id, rc);
            }
            continue;
        }

        message_header_t header;
        void *payload = NULL;
        rc = ipc_recv(client_fd, &header, &payload);
        if (rc != PROJECT_OK) {
            fprintf(stderr, "node %d: recv failed (err %d)\n",
                    node->node_id, rc);
            ipc_close(client_fd);
            continue;
        }

        queue_message_t msg;
        msg.header = header;
        msg.payload = payload;
        msg.client_fd = client_fd;

        rc = node_queue_push(&node->queue, &msg);
        if (rc != PROJECT_OK) {
            free(payload);
            ipc_close(client_fd);
        }
    }
    return NULL;
}

static void handle_block_proposal(node_t *node, queue_message_t *msg) {


    char *line = malloc(msg->header.payload_len + 1);
    if (line == NULL) {
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "out of memory", 13);
        return;
    }
    memcpy(line, msg->payload, msg->header.payload_len);
    line[msg->header.payload_len] = '\0';

    block_t proposed;
    block_init(&proposed);
    int rc = block_from_csv(line, &proposed);
    free(line);

    if (rc != PROJECT_OK) {
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "malformed block", 16);
        return;
    }

    pthread_mutex_lock(&node->chain_lock);

    const block_t *last = blockchain_get_by_index(&node->chain, node->chain.count - 1);

    int valid = validate_block_structure(&proposed) == PROJECT_OK
             && validate_block_merkle_root(&proposed) == PROJECT_OK
             && (last == NULL || validate_block_link(last, &proposed) == PROJECT_OK);

    if (!valid) {
        pthread_mutex_unlock(&node->chain_lock);
        block_destroy(&proposed);
        ipc_send(msg->client_fd, MSG_BLOCK_REJECT, (uint32_t)node->node_id,
                 "invalid proposal", 17);
        return;
    }

    rc = blockchain_append(&node->chain, &proposed);
    pthread_mutex_unlock(&node->chain_lock);

    block_destroy(&proposed);

    if (rc != PROJECT_OK) {
        ipc_send(msg->client_fd, MSG_BLOCK_REJECT, (uint32_t)node->node_id,
                 "append failed", 14);
        return;
    }

    ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id, NULL, 0);
    printf("node %d: block accepted and appended\n", node->node_id);

    /* TODO: broadcast MSG_BLOCK_COMMIT to other Nodes/Miners — next step */
}

static void handle_message(node_t *node, queue_message_t *msg) {
    printf("node %d: worker handling message type=%u sender=%u payload_len=%u\n",
           node->node_id, msg->header.type, msg->header.sender_id,
           msg->header.payload_len);

    switch (msg->header.type) {
        case MSG_BLOCK_PROPOSAL:
            if (node->is_coordinator) {
                handle_block_proposal(node, msg);
            } else {
                ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                         "not coordinator", 16);
            }
            break;
        default:
            ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id,
                      NULL, 0);
            break;
    }

    free(msg->payload);
    ipc_close(msg->client_fd);
}


static void *worker_main(void *arg) {
    node_t *node = (node_t *)arg;

    for (;;) {
        queue_message_t msg;
        int rc = node_queue_pop(&node->queue, &msg);
        if (rc == ERR_NOT_FOUND) {
            break;
        }
        if (rc != PROJECT_OK) {
            continue;
        }
        handle_message(node, &msg);
    }
    return NULL;
}

int node_init(node_t *node, int node_id, const char *runtime_dir) {
    if (node == NULL || runtime_dir == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    memset(node, 0, sizeof(*node));
    node->node_id = node_id;
    node->is_coordinator = (node_id == 0);
    strncpy(node->runtime_dir, runtime_dir, sizeof(node->runtime_dir) - 1);

    int rc = ipc_build_socket_path(runtime_dir, NODE_SOCK_FORMAT, node_id,
                                    node->sock_path, sizeof(node->sock_path));
    if (rc != PROJECT_OK) {
        return rc;
    }

    ipc_unlink_socket(node->sock_path);

    rc = ipc_server_create(node->sock_path, &node->server_fd);
    if (rc != PROJECT_OK) {
        return rc;
    }

    rc = blockchain_init(&node->chain);
    if (rc != PROJECT_OK) {
        ipc_close(node->server_fd);
        return rc;
    }
    pthread_mutex_init(&node->chain_lock, NULL);

    rc = node_queue_init(&node->queue);
    if (rc != PROJECT_OK) {
        blockchain_destroy(&node->chain);
        ipc_close(node->server_fd);
        return rc;
    }

    node->running = 1;
    return PROJECT_OK;
}

void node_stop(node_t *node) {
    if (node == NULL) {
        return;
    }
    node->running = 0;
    node_queue_shutdown(&node->queue);
    shutdown(node->server_fd, SHUT_RDWR);
}

int node_run(node_t *node) {
    if (node == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    printf("node %d: listening on %s (coordinator=%d)\n",
           node->node_id, node->sock_path, node->is_coordinator);

    if (pthread_create(&node->receiver_thread, NULL, receiver_main, node) != 0) {
        return ERR_PROCESS;
    }

    for (int i = 0; i < NODE_WORKER_COUNT; i++) {
        if (pthread_create(&node->workers[i], NULL, worker_main, node) != 0) {
            return ERR_PROCESS;
        }
    }

    pthread_join(node->receiver_thread, NULL);
    for (int i = 0; i < NODE_WORKER_COUNT; i++) {
        pthread_join(node->workers[i], NULL);
    }

    return PROJECT_OK;
}

void node_destroy(node_t *node) {
    if (node == NULL) {
        return;
    }
    node_queue_destroy(&node->queue);
    pthread_mutex_destroy(&node->chain_lock);
    blockchain_destroy(&node->chain);
    ipc_close(node->server_fd);
    ipc_unlink_socket(node->sock_path);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <node_id> <runtime_dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    node_t node;
    int rc = node_init(&node, atoi(argv[1]), argv[2]);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "node: init failed (err %d)\n", rc);
        return EXIT_FAILURE;
    }

    rc = node_run(&node);

    node_destroy(&node);
    return (rc == PROJECT_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
