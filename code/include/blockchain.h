#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <stddef.h>
#include <stdint.h>

#include "block.h"

typedef struct {
    block_t *blocks;
    size_t count;
    size_t capacity;
} blockchain_t;

int blockchain_init(blockchain_t *chain);

void blockchain_destroy(blockchain_t *chain);

int blockchain_append(
    blockchain_t *chain,
    const block_t *block
);

const block_t *blockchain_get_by_index(
    const blockchain_t *chain,
    uint64_t index
);

const block_t *blockchain_get_by_hash(
    const blockchain_t *chain,
    const char *hash
);

int blockchain_load_csv(
    blockchain_t *chain,
    const char *path
);

int blockchain_save_csv(
    const blockchain_t *chain,
    const char *path
);

#endif