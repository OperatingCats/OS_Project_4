#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

#include "block.h"

#define SHA256_DIGEST_SIZE 32

int sha256_bytes(
    const void *data,
    size_t data_size,
    unsigned char output[SHA256_DIGEST_SIZE]
);

int sha256_hex(
    const void *data,
    size_t data_size,
    char output[SHA256_HEX_STRING_SIZE]
);

int calculate_merkle_root(
    const char *const transactions[],
    size_t transaction_count,
    char output[SHA256_HEX_STRING_SIZE]
);

int build_block_hash_input(
    const block_t *block,
    char **output
);

int calculate_block_hash(
    const block_t *block,
    char output[SHA256_HEX_STRING_SIZE]
);

#endif