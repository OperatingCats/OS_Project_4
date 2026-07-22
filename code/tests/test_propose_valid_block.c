#include "ipc.h"
#include "errors.h"
#include "block.h"
#include "blockchain.h"
#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <socket_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

  

/* Query the running Node for the real block 0, instead of hand-typing
     * it, so our hash matches exactly what the Node has on file. */
    int query_fd;
    if (ipc_connect(argv[1], &query_fd) != PROJECT_OK) {
        fprintf(stderr, "connect (query) failed\n");
        return EXIT_FAILURE;
    }
    ipc_send(query_fd, MSG_QUERY_BLOCK, 1, "--index 0", 9);
    message_header_t qheader;
    void *qpayload = NULL;
    if (ipc_recv(query_fd, &qheader, &qpayload) != PROJECT_OK || qpayload == NULL) {
        fprintf(stderr, "failed to query genesis block\n");
        ipc_close(query_fd);
        return EXIT_FAILURE;
    }
    ipc_close(query_fd);

    char *genesis_csv_buf = malloc(qheader.payload_len + 1);
    memcpy(genesis_csv_buf, qpayload, qheader.payload_len);
    genesis_csv_buf[qheader.payload_len] = '\0';
    free(qpayload);

    block_t genesis;
    block_init(&genesis);
    if (block_from_csv(genesis_csv_buf, &genesis) != PROJECT_OK) {
        fprintf(stderr, "failed to parse queried genesis block\n");
        free(genesis_csv_buf);
        return EXIT_FAILURE;
    }
    free(genesis_csv_buf);


    char genesis_hash[SHA256_HEX_STRING_SIZE];
    if (calculate_block_hash(&genesis, genesis_hash) != PROJECT_OK) {
        fprintf(stderr, "failed to hash genesis block\n");
        return EXIT_FAILURE;
    }
    printf("DEBUG genesis_hash: %s\n", genesis_hash);

    /* Building a valid block 1. */
    block_t next;
    block_init(&next);
    next.index = 1;
    next.timestamp = genesis.timestamp + 1;
    strcpy(next.previous_hash, genesis_hash);
    next.nonce = 0;

    if (block_add_transaction(&next, "Alice pays Bob 5 coins") != PROJECT_OK) {
        fprintf(stderr, "failed to add transaction\n");
        return EXIT_FAILURE;
    }

    const char *tx = next.transactions[0];
    if (calculate_merkle_root(&tx, 1, next.merkle_root) != PROJECT_OK) {
        fprintf(stderr, "failed to compute merkle root\n");
        return EXIT_FAILURE;
    }
    printf("DEBUG merkle_root: %s\n", next.merkle_root);
    printf("DEBUG csv line will be built next\n");

    char *csv_line = NULL;
    if (block_to_csv(&next, &csv_line) != PROJECT_OK || csv_line == NULL) {
        fprintf(stderr, "failed to serialize block\n");
        return EXIT_FAILURE;
    }

    int fd;
    if (ipc_connect(argv[1], &fd) != PROJECT_OK) {
        fprintf(stderr, "connect failed\n");
        free(csv_line);
        return EXIT_FAILURE;
    }

    printf("DEBUG csv_line: %s\n", csv_line);
    ipc_send(fd, MSG_BLOCK_PROPOSAL, 1, csv_line, (uint32_t)strlen(csv_line));

    message_header_t header;
    void *payload = NULL;
    if (ipc_recv(fd, &header, &payload) == PROJECT_OK) {
        printf("reply type=%u payload_len=%u\n", header.type, header.payload_len);
        if (payload != NULL) {
            printf("payload: %.*s\n", header.payload_len, (char *)payload);
            free(payload);
        }
    }

    ipc_close(fd);
    free(csv_line);
    block_destroy(&genesis);
    block_destroy(&next);
    return EXIT_SUCCESS;
}
