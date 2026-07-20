#include "block.h"
#include "errors.h"

#include <stdlib.h>
#include <string.h>

int block_init(block_t *block)
{
    if (block == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    block->index = 0;
    block->timestamp = 0;
    block->nonce = 0;

    block->previous_hash[0] = '\0';
    block->merkle_root[0] = '\0';

    block->transactions = NULL;
    block->transaction_count = 0;

    return PROJECT_OK;
}

void block_destroy(block_t *block)
{
    if (block == NULL) {
        return;
    }

    for (size_t index = 0; index < block->transaction_count; index++) {
        free(block->transactions[index]);
    }

    free(block->transactions);

    block->transactions = NULL;
    block->transaction_count = 0;

    block->index = 0;
    block->timestamp = 0;
    block->nonce = 0;

    block->previous_hash[0] = '\0';
    block->merkle_root[0] = '\0';
}

int block_add_transaction(
    block_t *block,
    const char *transaction
)
{
    char **resized_transactions;
    char *transaction_copy;
    size_t transaction_length;

    if (block == NULL || transaction == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    transaction_length = strlen(transaction);

    if (transaction_length == 0 ||
        transaction_length >= MAX_TRANSACTION_LENGTH) {
        return ERR_INVALID_TRANSACTION;
    }

    if (block->transaction_count >= MAX_TRANSACTIONS_PER_BLOCK) {
        return ERR_INVALID_BLOCK;
    }

    transaction_copy = malloc(transaction_length + 1);

    if (transaction_copy == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    memcpy(
        transaction_copy,
        transaction,
        transaction_length + 1
    );

    resized_transactions = realloc(
        block->transactions,
        sizeof(char *) * (block->transaction_count + 1)
    );

    if (resized_transactions == NULL) {
        free(transaction_copy);
        return ERR_MEMORY_ALLOCATION;
    }

    block->transactions = resized_transactions;
    block->transactions[block->transaction_count] = transaction_copy;
    block->transaction_count++;

    return PROJECT_OK;
}

int block_copy(
    block_t *destination,
    const block_t *source
)
{
    int result;

    if (destination == NULL || source == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    /*
     * The destination must either be freshly initialized or safely
     * destroyable before being overwritten.
     */
    block_destroy(destination);

    result = block_init(destination);

    if (result != PROJECT_OK) {
        return result;
    }

    destination->index = source->index;
    destination->timestamp = source->timestamp;
    destination->nonce = source->nonce;

    memcpy(
        destination->previous_hash,
        source->previous_hash,
        SHA256_HEX_STRING_SIZE
    );

    memcpy(
        destination->merkle_root,
        source->merkle_root,
        SHA256_HEX_STRING_SIZE
    );

    for (size_t index = 0;
         index < source->transaction_count;
         index++) {
        result = block_add_transaction(
            destination,
            source->transactions[index]
        );

        if (result != PROJECT_OK) {
            block_destroy(destination);
            return result;
        }
    }

    return PROJECT_OK;
}