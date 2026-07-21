

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"
#include "ipc.h"
#include "errors.h"
#include "node_queue.h"

/* Usage: node <node_id> <runtime_dir>
 * runtime_dir is the /tmp/blockchain_<pid> directory created by bootstrap.
 * Node 0 is the coordinator. */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <node_id> <runtime_dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int node_id = atoi(argv[1]);
    const char *runtime_dir = argv[2];

    char sock_path[MAX_SOCKET_PATH_LEN];
    int rc = ipc_build_socket_path(runtime_dir, NODE_SOCK_FORMAT, node_id,
                                    sock_path, sizeof(sock_path));
    if (rc != PROJECT_OK) {
        fprintf(stderr, "node %d: failed to build socket path (err %d)\n",
                node_id, rc);
        return EXIT_FAILURE;
    }

    /* Clean up any stale socket file from a previous crashed run. */
    ipc_unlink_socket(sock_path);

    int server_fd;
    rc = ipc_server_create(sock_path, &server_fd);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "node %d: failed to create socket at %s (err %d)\n",
                node_id, sock_path, rc);
        return EXIT_FAILURE;
    }

    printf("node %d: listening on %s\n", node_id, sock_path);

    node_queue_t queue;
    node_queue_init(&queue);

    /* TEMP: single-threaded accept loop for now, just to prove the
     * socket + queue wiring works end to end. Thread pool comes next. */
    for (;;) {
        int client_fd;
        rc = ipc_server_accept(server_fd, &client_fd);
        if (rc != PROJECT_OK) {
            fprintf(stderr, "node %d: accept failed (err %d)\n", node_id, rc);
            continue;
        }

        message_header_t header;
        void *payload = NULL;
        rc = ipc_recv(client_fd, &header, &payload);
        if (rc != PROJECT_OK) {
            fprintf(stderr, "node %d: recv failed (err %d)\n", node_id, rc);
            ipc_close(client_fd);
            continue;
        }

        printf("node %d: received message type=%u sender=%u payload_len=%u\n",
               node_id, header.type, header.sender_id, header.payload_len);

        free(payload);
        ipc_close(client_fd);
    }

    node_queue_destroy(&queue);
    ipc_unlink_socket(sock_path);
    return EXIT_SUCCESS;
}/* Node executable: validation, blockchain replication, coordinator role for Node 0.*/
