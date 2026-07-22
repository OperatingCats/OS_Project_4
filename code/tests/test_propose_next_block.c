#include "ipc.h"
#include "errors.h"
#include "block.h"
#include "blockchain.h"
#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <socket_path> [transaction text]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *tx_text = (argc >= 3) ? argv[2] : "Alice pays Bob 5 coins";

    /* Query current height, then the block at height-1 (the current tip). */
    int fd;
    if (ipc_connect(argv[1], &fd) != PROJECT_OK) {
        fprintf(stderr, "connect (height) failed\n");
        return EXIT_FAILURE;
    }
    ipc_send(fd, MSG_QUERY_HEIGHT, 1, NULL, 0);
    message_header_t hheader;
    void *hpayload = NULL;
    if (ipc_recv(fd, &hheader, &hpayload) != PROJECT_OK || hpayload == NULL) {
        fprintf(stderr, "height query failed\n");
        return EXIT_FAILURE;
    }
    ipc_close(fd);
    char hbuf[32];
    memcpy(hbuf, hpayload, hheader.payload_len < 31 ? hheader.payload_len : 31);
    hbuf[hheader.payload_len < 31 ? hheader.payload_len : 31] = '\0';
    free(hpayload);
    unsigned long long height = strtoull(hbuf, NULL, 10);
    if (height == 0) {
        fprintf(stderr, "chain is empty\n");
        return EXIT_FAILURE;
    }

    int qfd;
    if (ipc_connect(argv[1], &qfd) != PROJECT_OK) {
        fprintf(stderr, "connect (tip) failed\n");
        return EXIT_FAILURE;
    }
    char idx_query[32];
    int idx_len = snprintf(idx_query, sizeof(idx_query), "--index %llu", height - 1);
    ipc_send(qfd, MSG_QUERY_BLOCK, 1, idx_query, (uint32_t)idx_len);
    message_header_t bheader;
    void *bpayload = NULL;
    if (ipc_recv(qfd, &bheader, &bpayload) != PROJECT_OK || bpayload == NULL) {
        fprintf(stderr, "tip query failed\n");
        return EXIT_FAILURE;
    }
    ipc_close(qfd);
    char *tip_csv = malloc(bheader.payload_len + 1);
    memcpy(tip_csv, bpayload, bheader.payload_len);
    tip_csv[bheader.payload_len] = '\0';
    free(bpayload);

    block_t tip;
    block_init(&tip);
    if (block_from_csv(tip_csv, &tip) != PROJECT_OK) {
        fprintf(stderr, "failed to parse tip block\n");
        free(tip_csv);
        return EXIT_FAILURE;
    }
    free(tip_csv);

    char tip_hash[SHA256_HEX_STRING_SIZE];
    if (calculate_block_hash(&tip, tip_hash) != PROJECT_OK) {
        fprintf(stderr, "failed to hash tip block\n");
        return EXIT_FAILURE;
    }

    block_t next;
    block_init(&next);
    next.index = tip.index + 1;
    next.timestamp = tip.timestamp + 1;
    strcpy(next.previous_hash, tip_hash);
    next.nonce = 0;

    if (block_add_transaction(&next, tx_text) != PROJECT_OK) {
        fprintf(stderr, "failed to add transaction\n");
        return EXIT_FAILURE;
    }
    const char *tx_ptr = next.transactions[0];
    if (calculate_merkle_root(&tx_ptr, 1, next.merkle_root) != PROJECT_OK) {
        fprintf(stderr, "failed to compute merkle root\n");
        return EXIT_FAILURE;
    }

    char *csv_line = NULL;
    if (block_to_csv(&next, &csv_line) != PROJECT_OK || csv_line == NULL) {
        fprintf(stderr, "failed to serialize block\n");
        return EXIT_FAILURE;
    }

    int pfd;
    if (ipc_connect(argv[1], &pfd) != PROJECT_OK) {
        fprintf(stderr, "connect (propose) failed\n");
        free(csv_line);
        return EXIT_FAILURE;
    }
    ipc_send(pfd, MSG_BLOCK_PROPOSAL, 1, csv_line, (uint32_t)strlen(csv_line));
    message_header_t rheader;
    void *rpayload = NULL;
    if (ipc_recv(pfd, &rheader, &rpayload) == PROJECT_OK) {
        printf("reply type=%u payload_len=%u\n", rheader.type, rheader.payload_len);
        if (rpayload != NULL) {
            printf("payload: %.*s\n", rheader.payload_len, (char *)rpayload);
            free(rpayload);
        }
    }
    ipc_close(pfd);
    free(csv_line);
    block_destroy(&tip);
    block_destroy(&next);
    return EXIT_SUCCESS;
}
