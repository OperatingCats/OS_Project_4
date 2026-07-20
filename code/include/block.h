#ifndef BLOCK_H
#define BLOCK_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_HEX_LENGTH 64
#define SHA256_HEX_STRING_SIZE 65

#define MAX_TRANSACTION_LENGTH 256
#define MAX_TRANSACTIONS_PER_BLOCK 16

typedef struct {
    uint64_t index;
    uint64_t timestamp;

    char previous_hash[SHA256_HEX_STRING_SIZE];
    char merkle_root[SHA256_HEX_STRING_SIZE];

    uint64_t nonce;

    char **transactions;
    size_t transaction_count;
} block_t;

int block_init(block_t *block);

void block_destroy(block_t *block);

int block_copy(
    block_t *destination,
    const block_t *source
);

int block_add_transaction(
    block_t *block,
    const char *transaction
);

int block_to_csv(
    const block_t *block,
    char **output
);

int block_from_csv(
    const char *line,
    block_t *output
);

int block_serialize(
    const block_t *block,
    unsigned char **buffer,
    size_t *buffer_size
);

int block_deserialize(
    const unsigned char *buffer,
    size_t buffer_size,
    block_t *output
);

#endif