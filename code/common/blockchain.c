#include "blockchain.h"

#include "crypto.h"
#include "errors.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_BLOCKCHAIN_CAPACITY 4

int blockchain_init(blockchain_t *chain)
{
    if (chain == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    chain->blocks = NULL;
    chain->count = 0;
    chain->capacity = 0;

    return PROJECT_OK;
}

void blockchain_destroy(blockchain_t *chain)
{
    if (chain == NULL) {
        return;
    }

    for (size_t index = 0; index < chain->count; index++) {
        block_destroy(&chain->blocks[index]);
    }

    free(chain->blocks);

    chain->blocks = NULL;
    chain->count = 0;
    chain->capacity = 0;
}

static int blockchain_grow(blockchain_t *chain)
{
    size_t new_capacity;
    block_t *new_blocks;

    if (chain == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (chain->capacity == 0) {
        new_capacity = INITIAL_BLOCKCHAIN_CAPACITY;
    } else {
        new_capacity = chain->capacity * 2;
    }

    new_blocks = realloc(
        chain->blocks,
        new_capacity * sizeof(*new_blocks)
    );

    if (new_blocks == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    chain->blocks = new_blocks;
    chain->capacity = new_capacity;

    return PROJECT_OK;
}

int blockchain_append(
    blockchain_t *chain,
    const block_t *block
)
{
    int result;

    if (chain == NULL || block == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (chain->count == chain->capacity) {
        result = blockchain_grow(chain);

        if (result != PROJECT_OK) {
            return result;
        }
    }

    result = block_init(&chain->blocks[chain->count]);

    if (result != PROJECT_OK) {
        return result;
    }

    result = block_copy(
        &chain->blocks[chain->count],
        block
    );

    if (result != PROJECT_OK) {
        block_destroy(&chain->blocks[chain->count]);
        return result;
    }

    chain->count++;

    return PROJECT_OK;
}

const block_t *blockchain_get_by_index(
    const blockchain_t *chain,
    uint64_t index
)
{
    if (chain == NULL) {
        return NULL;
    }

    for (size_t position = 0;
         position < chain->count;
         position++) {
        if (chain->blocks[position].index == index) {
            return &chain->blocks[position];
        }
    }

    return NULL;
}

const block_t *blockchain_get_by_hash(
    const blockchain_t *chain,
    const char *hash
)
{
    char calculated_hash[SHA256_HEX_STRING_SIZE];

    if (chain == NULL || hash == NULL) {
        return NULL;
    }

    if (strlen(hash) != SHA256_HEX_LENGTH) {
        return NULL;
    }

    for (size_t position = 0;
         position < chain->count;
         position++) {
        int result = calculate_block_hash(
            &chain->blocks[position],
            calculated_hash
        );

        if (result != PROJECT_OK) {
            continue;
        }

        if (strcmp(calculated_hash, hash) == 0) {
            return &chain->blocks[position];
        }
    }

    return NULL;
}