#include "ipc.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Queries a specific block by index from a "source" Node, then relays
 * that exact block to a "target" Node as a MSG_BLOCK_COMMIT — used to
 * simulate a commit broadcast reaching a Node that's behind. */

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source_socket> <target_socket> <index>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *source_sock = argv[1];
    const char *target_sock = argv[2];
    const char *index = argv[3];

    int sfd;
    if (ipc_connect(source_sock, &sfd) != PROJECT_OK) {
        fprintf(stderr, "connect (source) failed\n");
        return EXIT_FAILURE;
    }
    char query[32];
    int qlen = snprintf(query, sizeof(query), "--index %s", index);
    ipc_send(sfd, MSG_QUERY_BLOCK, 1, query, (uint32_t)qlen);
    message_header_t qheader;
    void *qpayload = NULL;
    if (ipc_recv(sfd, &qheader, &qpayload) != PROJECT_OK || qpayload == NULL) {
        fprintf(stderr, "query failed\n");
        return EXIT_FAILURE;
    }
    ipc_close(sfd);

    int tfd;
    if (ipc_connect(target_sock, &tfd) != PROJECT_OK) {
        fprintf(stderr, "connect (target) failed\n");
        free(qpayload);
        return EXIT_FAILURE;
    }
    ipc_send(tfd, MSG_BLOCK_COMMIT, 1, qpayload, qheader.payload_len);
    free(qpayload);
    ipc_close(tfd);

    printf("relay: sent block %s from source to target as commit\n", index);
    return EXIT_SUCCESS;
}
