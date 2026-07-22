#include "validation.h"
#include "errors.h"
#include "crypto.h"

#include <ctype.h>
#include <regex.h>
#include <stddef.h>
#include <string.h>

int validate_transaction(const char *transaction)
{
    static const char pattern[] =
        "^[[:alnum:]]+ pays [[:alnum:]]+ "
        "[1-9][0-9]* coins$";

    regex_t regex;
    int regex_result;
    size_t transaction_length;

    if (transaction == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    transaction_length = strlen(transaction);

    if (transaction_length == 0 ||
        transaction_length >= MAX_TRANSACTION_LENGTH) {
        return ERR_INVALID_TRANSACTION;
    }

    regex_result = regcomp(
        &regex,
        pattern,
        REG_EXTENDED | REG_NOSUB
    );

    if (regex_result != 0) {
        return ERR_INVALID_TRANSACTION;
    }

    regex_result = regexec(
        &regex,
        transaction,
        0,
        NULL,
        0
    );

    regfree(&regex);

    if (regex_result != 0) {
        return ERR_INVALID_TRANSACTION;
    }

    return PROJECT_OK;
}

int validate_hash_string(const char *hash)
{
    if (hash == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (strlen(hash) != SHA256_HEX_LENGTH) {
        return ERR_INVALID_BLOCK;
    }

    for (size_t index = 0;
         index < SHA256_HEX_LENGTH;
         index++) {
        unsigned char character =
            (unsigned char)hash[index];

        /*
         * All hashes produced by the project use lowercase
         * hexadecimal characters.
         */
        if (!isdigit(character) &&
            !(character >= 'a' && character <= 'f')) {
            return ERR_INVALID_BLOCK;
        }
    }

    return PROJECT_OK;
}

int validate_block_structure(const block_t *block)
{
    if (block == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (validate_hash_string(
            block->previous_hash
        ) != PROJECT_OK) {
        return ERR_INVALID_BLOCK;
    }

    if (validate_hash_string(
            block->merkle_root
        ) != PROJECT_OK) {
        return ERR_INVALID_BLOCK;
    }

    if (block->transactions == NULL ||
        block->transaction_count == 0 ||
        block->transaction_count >
            MAX_TRANSACTIONS_PER_BLOCK) {
        return ERR_INVALID_BLOCK;
    }

    /*
     * The official specification exempts the genesis block from
     * the normal transaction regular expression. Its payload may
     * contain an arbitrary string such as "Genesis block".
     *
     * The payload must still be present, non-empty, and shorter
     * than MAX_TRANSACTION_LENGTH.
     */
    if (block->index == 0) {
        for (size_t index = 0;
             index < block->transaction_count;
             index++) {
            const char *transaction =
                block->transactions[index];

            if (transaction == NULL) {
                return ERR_INVALID_BLOCK;
            }

            size_t transaction_length =
                strlen(transaction);

            if (transaction_length == 0 ||
                transaction_length >=
                    MAX_TRANSACTION_LENGTH) {
                return ERR_INVALID_BLOCK;
            }
        }

        return PROJECT_OK;
    }

    /*
     * Every transaction in a normal block must follow the
     * required transaction format.
     */
    for (size_t index = 0;
         index < block->transaction_count;
         index++) {
        if (validate_transaction(
                block->transactions[index]
            ) != PROJECT_OK) {
            return ERR_INVALID_BLOCK;
        }
    }

    return PROJECT_OK;
}

int validate_block_merkle_root(const block_t *block)
{
    char calculated_root[SHA256_HEX_STRING_SIZE];
    int result;

    if (block == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    result = validate_block_structure(block);

    if (result != PROJECT_OK) {
        return result;
    }

    result = calculate_merkle_root(
        (const char *const *)block->transactions,
        block->transaction_count,
        calculated_root
    );

    if (result != PROJECT_OK) {
        return result;
    }

    if (strcmp(
            calculated_root,
            block->merkle_root
        ) != 0) {
        return ERR_INVALID_BLOCK;
    }

    return PROJECT_OK;
}

int validate_block_link(
    const block_t *previous,
    const block_t *current
)
{
    char previous_block_hash[SHA256_HEX_STRING_SIZE];
    int result;

    if (previous == NULL || current == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    /*
     * Revalidate both blocks before checking their relationship.
     * The genesis exception is handled inside
     * validate_block_structure().
     */
    result = validate_block_merkle_root(previous);

    if (result != PROJECT_OK) {
        return ERR_INVALID_BLOCK;
    }

    result = validate_block_merkle_root(current);

    if (result != PROJECT_OK) {
        return ERR_INVALID_BLOCK;
    }

    if (current->index != previous->index + 1) {
        return ERR_INVALID_BLOCKCHAIN;
    }

    if (current->timestamp < previous->timestamp) {
        return ERR_INVALID_BLOCKCHAIN;
    }

    result = calculate_block_hash(
        previous,
        previous_block_hash
    );

    if (result != PROJECT_OK) {
        return result;
    }

    if (strcmp(
            current->previous_hash,
            previous_block_hash
        ) != 0) {
        return ERR_INVALID_BLOCKCHAIN;
    }

    return PROJECT_OK;
}

int validate_blockchain(const blockchain_t *chain)
{
    int result;

    if (chain == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (chain->blocks == NULL ||
        chain->count == 0) {
        return ERR_INVALID_BLOCKCHAIN;
    }

    /*
     * A complete blockchain must start from index 0.
     */
    if (chain->blocks[0].index != 0) {
        return ERR_INVALID_BLOCKCHAIN;
    }

    for (size_t index = 0;
         index < chain->count;
         index++) {
        result = validate_block_merkle_root(
            &chain->blocks[index]
        );

        if (result != PROJECT_OK) {
            return ERR_INVALID_BLOCKCHAIN;
        }
    }

    for (size_t index = 1;
         index < chain->count;
         index++) {
        result = validate_block_link(
            &chain->blocks[index - 1],
            &chain->blocks[index]
        );

        if (result != PROJECT_OK) {
            return ERR_INVALID_BLOCKCHAIN;
        }
    }

    return PROJECT_OK;
}