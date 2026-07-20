#ifndef VALIDATION_H
#define VALIDATION_H

#include "block.h"
#include "blockchain.h"

int validate_transaction(
    const char *transaction
);

int validate_hash_string(
    const char *hash
);

int validate_block_structure(
    const block_t *block
);

int validate_block_merkle_root(
    const block_t *block
);

int validate_block_link(
    const block_t *previous,
    const block_t *current
);

int validate_blockchain(
    const blockchain_t *chain
);

#endif