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
#include <signal.h>

static node_t *g_node_for_signal = NULL;

static void handle_termination_signal(int signum) {
    (void)signum;
    if (g_node_for_signal != NULL) {
        node_stop(g_node_for_signal);
    }
}

static void broadcast_commit(node_t *node, const block_t *committed);
static void handle_sync_request(node_t *node, queue_message_t *msg);
static void handle_block_commit(node_t *node, queue_message_t *msg);

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
	logger_log(node->log_file, "node", node->node_id,
                   "block proposal rejected: invalid proposal");
        return;
    }

    rc = blockchain_append(&node->chain, &proposed);
    pthread_mutex_unlock(&node->chain_lock);

    block_destroy(&proposed);

    if (rc != PROJECT_OK) {
        ipc_send(msg->client_fd, MSG_BLOCK_REJECT, (uint32_t)node->node_id,
                 "append failed", 14);
	logger_log(node->log_file, "node", node->node_id,
                   "block proposal rejected: append failed");
        return;
    }

    ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id, NULL, 0);
    printf("node %d: block accepted and appended\n", node->node_id);
    logger_log(node->log_file, "node", node->node_id,
               "block %llu accepted and appended",
               (unsigned long long)proposed.index);
    pthread_mutex_lock(&node->chain_lock);
    const block_t *last_committed = blockchain_get_by_index(&node->chain, node->chain.count - 1);
    if (last_committed != NULL) {
        broadcast_commit(node, last_committed);
    }
    pthread_mutex_unlock(&node->chain_lock);
}



#define MAX_PROBE_PEERS 16


static void broadcast_commit(node_t *node, const block_t *committed) {
    char *line = NULL;
    if (block_to_csv(committed, &line) != PROJECT_OK || line == NULL) {
        return;
    }

    for (int id = 0; id < MAX_PROBE_PEERS; id++) {
        if (id == node->node_id) {
            continue;
        }
        char peer_path[MAX_SOCKET_PATH_LEN];
        if (ipc_build_socket_path(node->runtime_dir, NODE_SOCK_FORMAT, id,
                                   peer_path, sizeof(peer_path)) != PROJECT_OK) {
            continue;
        }
        int peer_fd;
        if (ipc_connect(peer_path, &peer_fd) == PROJECT_OK) {
            ipc_send(peer_fd, MSG_BLOCK_COMMIT, (uint32_t)node->node_id,
                      line, (uint32_t)strlen(line));
            ipc_close(peer_fd);
        }
    }

    for (int id = 0; id < MAX_PROBE_PEERS; id++) {
        char peer_path[MAX_SOCKET_PATH_LEN];
        if (ipc_build_socket_path(node->runtime_dir, MINER_SOCK_FORMAT, id,
                                   peer_path, sizeof(peer_path)) != PROJECT_OK) {
            continue;
        }
        int peer_fd;
        if (ipc_connect(peer_path, &peer_fd) == PROJECT_OK) {
            ipc_send(peer_fd, MSG_BLOCK_COMMIT, (uint32_t)node->node_id,
                      line, (uint32_t)strlen(line));
            ipc_close(peer_fd);
        }
    }

    free(line);
    logger_log(node->log_file, "node", node->node_id,
               "broadcast commit for block %llu",
               (unsigned long long)committed->index);
}

static void handle_query_height(node_t *node, queue_message_t *msg) {
    pthread_mutex_lock(&node->chain_lock);
    uint64_t height = (uint64_t)node->chain.count;
    pthread_mutex_unlock(&node->chain_lock);

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)height);

    ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id,
              buf, (uint32_t)len);

    logger_log(node->log_file, "node", node->node_id,
               "query height answered: %llu", (unsigned long long)height);
}



static void handle_query_block(node_t *node, queue_message_t *msg) {
    /* payload format: "--index <n>" or "--hash <h>" */
    char *payload_str = malloc(msg->header.payload_len + 1);
    if (payload_str == NULL) {
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "out of memory", 13);
        return;
    }
    memcpy(payload_str, msg->payload, msg->header.payload_len);
    payload_str[msg->header.payload_len] = '\0';

    pthread_mutex_lock(&node->chain_lock);

    const block_t *found = NULL;
    unsigned long long idx;
    char hash[SHA256_HEX_STRING_SIZE];

    if (sscanf(payload_str, "--index %llu", &idx) == 1) {
        found = blockchain_get_by_index(&node->chain, (uint64_t)idx);
    } else if (sscanf(payload_str, "--hash %64s", hash) == 1) {
        found = blockchain_get_by_hash(&node->chain, hash);
    }

    if (found == NULL) {
        pthread_mutex_unlock(&node->chain_lock);
        free(payload_str);
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "block not found", 16);
        return;
    }

    char *line = NULL;
    int rc = block_to_csv(found, &line);
    pthread_mutex_unlock(&node->chain_lock);
    free(payload_str);

    if (rc != PROJECT_OK || line == NULL) {
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "serialization failed", 21);
        return;
    }

    ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id,
              line, (uint32_t)strlen(line));
    logger_log(node->log_file, "node", node->node_id, "query block answered");
    free(line);
}

static void handle_query_chain(node_t *node, queue_message_t *msg) {
    pthread_mutex_lock(&node->chain_lock);

    if (node->chain.count == 0) {
        pthread_mutex_unlock(&node->chain_lock);
        ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id, NULL, 0);
        return;
    }

    size_t buf_capacity = 4096;
    char *buf = malloc(buf_capacity);
    size_t buf_len = 0;
    if (buf == NULL) {
        pthread_mutex_unlock(&node->chain_lock);
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "out of memory", 13);
        return;
    }
    buf[0] = '\0';

    for (size_t i = 0; i < node->chain.count; i++) {
        char *line = NULL;
        int rc = block_to_csv(&node->chain.blocks[i], &line);
        if (rc != PROJECT_OK || line == NULL) {
            continue;
        }
        size_t line_len = strlen(line);
        while (buf_len + line_len + 2 > buf_capacity) {
            buf_capacity *= 2;
            char *grown = realloc(buf, buf_capacity);
            if (grown == NULL) {
                free(buf);
                free(line);
                pthread_mutex_unlock(&node->chain_lock);
                ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                         "out of memory", 13);
                return;
            }
            buf = grown;
        }
        memcpy(buf + buf_len, line, line_len);
        buf_len += line_len;
        buf[buf_len++] = '\n';
        buf[buf_len] = '\0';
        free(line);
    }

    pthread_mutex_unlock(&node->chain_lock);

    ipc_send(msg->client_fd, MSG_RESPONSE_OK, (uint32_t)node->node_id,
              buf, (uint32_t)buf_len);
    free(buf);
}


static void handle_sync_request(node_t *node, queue_message_t *msg) {
    char *payload_str = malloc(msg->header.payload_len + 1);
    if (payload_str == NULL) {
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "out of memory", 13);
        return;
    }
    memcpy(payload_str, msg->payload, msg->header.payload_len);
    payload_str[msg->header.payload_len] = '\0';

    unsigned long long from_index = 0;
    sscanf(payload_str, "%llu", &from_index);
    free(payload_str);

    pthread_mutex_lock(&node->chain_lock);

    size_t buf_capacity = 4096;
    char *buf = malloc(buf_capacity);
    size_t buf_len = 0;
    if (buf == NULL) {
        pthread_mutex_unlock(&node->chain_lock);
        ipc_send(msg->client_fd, MSG_RESPONSE_ERR, (uint32_t)node->node_id,
                 "out of memory", 13);
        return;
    }
    buf[0] = '\0';

    for (size_t i = (size_t)from_index; i < node->chain.count; i++) {
        char *line = NULL;
        int rc = block_to_csv(&node->chain.blocks[i], &line);
        if (rc != PROJECT_OK || line == NULL) {
            continue;
        }
        size_t line_len = strlen(line);
        while (buf_len + line_len + 2 > buf_capacity) {
            buf_capacity *= 2;
            char *grown = realloc(buf, buf_capacity);
            if (grown == NULL) {
                free(buf);
                free(line);
                pthread_mutex_unlock(&node->chain_lock);
                return;
            }
            buf = grown;
        }
        memcpy(buf + buf_len, line, line_len);
        buf_len += line_len;
        buf[buf_len++] = '\n';
        buf[buf_len] = '\0';
        free(line);
    }

    pthread_mutex_unlock(&node->chain_lock);

    ipc_send(msg->client_fd, MSG_SYNC_RESPONSE, (uint32_t)node->node_id,
              buf, (uint32_t)buf_len);
    logger_log(node->log_file, "node", node->node_id,
               "sync request answered from index %llu", from_index);
    free(buf);
}



static void handle_block_commit(node_t *node, queue_message_t *msg) {
    char *line = malloc(msg->header.payload_len + 1);
    if (line == NULL) {
        return;
    }
    memcpy(line, msg->payload, msg->header.payload_len);
    line[msg->header.payload_len] = '\0';

    block_t incoming;
    block_init(&incoming);
    int rc = block_from_csv(line, &incoming);
    free(line);
    if (rc != PROJECT_OK) {
        return;
    }

    pthread_mutex_lock(&node->chain_lock);
    uint64_t expected_index = (uint64_t)node->chain.count;

    if (incoming.index == expected_index) {
        /* the next block: validate and append directly. */
        const block_t *last = (node->chain.count > 0)
            ? blockchain_get_by_index(&node->chain, node->chain.count - 1)
            : NULL;
        int valid = validate_block_structure(&incoming) == PROJECT_OK
                 && validate_block_merkle_root(&incoming) == PROJECT_OK
                 && (last == NULL || validate_block_link(last, &incoming) == PROJECT_OK);
        if (valid) {
            blockchain_append(&node->chain, &incoming);
            logger_log(node->log_file, "node", node->node_id,
                       "block %llu committed via broadcast",
                       (unsigned long long)incoming.index);
        }
        pthread_mutex_unlock(&node->chain_lock);
        block_destroy(&incoming);
        return;
    }

    if (incoming.index < expected_index) {
        /* Old/duplicate ignore. */
        pthread_mutex_unlock(&node->chain_lock);
        block_destroy(&incoming);
        logger_log(node->log_file, "node", node->node_id,
                   "duplicate/old block %llu ignored",
                   (unsigned long long)incoming.index);
        return;
    }

    /* incoming.index > expected_index: behind, need to sync. */
    pthread_mutex_unlock(&node->chain_lock);
    block_destroy(&incoming);

    char coordinator_path[MAX_SOCKET_PATH_LEN];
    if (ipc_build_socket_path(node->runtime_dir, NODE_SOCK_FORMAT, 0,
                               coordinator_path, sizeof(coordinator_path)) != PROJECT_OK) {
        return;
    }

    int fd;
    if (ipc_connect(coordinator_path, &fd) != PROJECT_OK) {
        return;
    }

    char request[32];
    int req_len = snprintf(request, sizeof(request), "%llu",
                            (unsigned long long)expected_index);
    ipc_send(fd, MSG_SYNC_REQUEST, (uint32_t)node->node_id, request, (uint32_t)req_len);

    message_header_t sync_header;
    void *sync_payload = NULL;
    if (ipc_recv(fd, &sync_header, &sync_payload) == PROJECT_OK && sync_payload != NULL) {
        char *sync_str = malloc(sync_header.payload_len + 1);
        if (sync_str != NULL) {
            memcpy(sync_str, sync_payload, sync_header.payload_len);
            sync_str[sync_header.payload_len] = '\0';

            char *saveptr = NULL;
            char *line_tok = strtok_r(sync_str, "\n", &saveptr);
            pthread_mutex_lock(&node->chain_lock);
            while (line_tok != NULL) {
                block_t synced;
                block_init(&synced);
                if (block_from_csv(line_tok, &synced) == PROJECT_OK) {
                    blockchain_append(&node->chain, &synced);
                }
                block_destroy(&synced);
                line_tok = strtok_r(NULL, "\n", &saveptr);
            }
            pthread_mutex_unlock(&node->chain_lock);
            free(sync_str);

            logger_log(node->log_file, "node", node->node_id, "recovery completed");
        }
        free(sync_payload);
    }
    ipc_close(fd);
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
	case MSG_QUERY_CHAIN:
            handle_query_chain(node, msg);
            break;
	case MSG_QUERY_HEIGHT:
            handle_query_height(node, msg);
            break;
        case MSG_QUERY_BLOCK:
            handle_query_block(node, msg);
            break;
	case MSG_SYNC_REQUEST:
            handle_sync_request(node, msg);
            break;
	case MSG_BLOCK_COMMIT:
            handle_block_commit(node, msg);
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
    node->log_file = logger_init(runtime_dir, "node", node_id);
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
    char genesis_path[300];
    int written = snprintf(genesis_path, sizeof(genesis_path),
                            "%s/initial_state.csv", runtime_dir);
    if (written > 0 && (size_t)written < sizeof(genesis_path)) {
        int load_rc = blockchain_load_csv(&node->chain, genesis_path);
        if (load_rc == PROJECT_OK) {
            printf("node %d: loaded genesis state from %s (%zu block(s))\n",
                   node_id, genesis_path, node->chain.count);
        } else {
            printf("node %d: no genesis state loaded from %s (err %d)\n",
                   node_id, genesis_path, load_rc);
        }
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
    logger_close(node->log_file);
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

    g_node_for_signal = &node;
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_termination_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    rc = node_run(&node);

    node_destroy(&node);
    return (rc == PROJECT_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
