#include "crypto.h"
#include "errors.h"
#include "sha256.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void digest_to_hex(
    const unsigned char digest[SHA256_DIGEST_SIZE],
    char output[SHA256_HEX_STRING_SIZE]
)
{
    static const char hexadecimal_digits[] = "0123456789abcdef";

    for (size_t index = 0; index < SHA256_DIGEST_SIZE; index++) {
        output[index * 2] =
            hexadecimal_digits[(digest[index] >> 4) & 0x0f];

        output[index * 2 + 1] =
            hexadecimal_digits[digest[index] & 0x0f];
    }

    output[SHA256_HEX_LENGTH] = '\0';
}

int sha256_bytes(
    const void *data,
    size_t data_size,
    unsigned char output[SHA256_DIGEST_SIZE]
)
{
    SHA256_CTX context;

    if (output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (data == NULL && data_size != 0) {
        return ERR_INVALID_ARGUMENT;
    }

    sha256_init(&context);

    /*
     * For an empty input, sha256_update does not need to be called.
     * This also avoids passing NULL to the third-party function.
     */
    if (data_size > 0) {
        sha256_update(
            &context,
            (const BYTE *)data,
            data_size
        );
    }

    sha256_final(&context, output);

    return PROJECT_OK;
}

int sha256_hex(
    const void *data,
    size_t data_size,
    char output[SHA256_HEX_STRING_SIZE]
)
{
    unsigned char digest[SHA256_DIGEST_SIZE];
    int result;

    if (output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    result = sha256_bytes(data, data_size, digest);

    if (result != PROJECT_OK) {
        return result;
    }

    digest_to_hex(digest, output);

    return PROJECT_OK;
}

int calculate_merkle_root(
    const char *const transactions[],
    size_t transaction_count,
    char output[SHA256_HEX_STRING_SIZE]
)
{
    char (*current_level)[SHA256_HEX_STRING_SIZE];
    size_t current_count;
    int result;

    if (transactions == NULL || output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (transaction_count == 0) {
        return ERR_INVALID_ARGUMENT;
    }

    current_level = calloc(
        transaction_count + 1,
        sizeof(*current_level)
    );

    if (current_level == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    for (size_t index = 0; index < transaction_count; index++) {
        if (transactions[index] == NULL) {
            free(current_level);
            return ERR_INVALID_TRANSACTION;
        }

        result = sha256_hex(
            transactions[index],
            strlen(transactions[index]),
            current_level[index]
        );

        if (result != PROJECT_OK) {
            free(current_level);
            return result;
        }
    }

    current_count = transaction_count;

    while (current_count > 1) {
        size_t even_count;
        size_t next_count;
        char (*next_level)[SHA256_HEX_STRING_SIZE];

        if (current_count % 2 != 0) {
            result = sha256_hex(
                "",
                0,
                current_level[current_count]
            );

            if (result != PROJECT_OK) {
                free(current_level);
                return result;
            }

            current_count++;
        }

        even_count = current_count;
        next_count = even_count / 2;

        next_level = calloc(
            next_count + 1,
            sizeof(*next_level)
        );

        if (next_level == NULL) {
            free(current_level);
            return ERR_MEMORY_ALLOCATION;
        }

        for (size_t index = 0; index < even_count; index += 2) {
            char combined_hashes[
                SHA256_HEX_LENGTH * 2 + 1
            ];

            memcpy(
                combined_hashes,
                current_level[index],
                SHA256_HEX_LENGTH
            );

            memcpy(
                combined_hashes + SHA256_HEX_LENGTH,
                current_level[index + 1],
                SHA256_HEX_LENGTH
            );

            combined_hashes[
                SHA256_HEX_LENGTH * 2
            ] = '\0';

            result = sha256_hex(
                combined_hashes,
                SHA256_HEX_LENGTH * 2,
                next_level[index / 2]
            );

            if (result != PROJECT_OK) {
                free(next_level);
                free(current_level);
                return result;
            }
        }

        free(current_level);
        current_level = next_level;
        current_count = next_count;
    }

    memcpy(
        output,
        current_level[0],
        SHA256_HEX_STRING_SIZE
    );

    free(current_level);

    return PROJECT_OK;
}

int build_block_hash_input(
    const block_t *block,
    char **output
)
{
    size_t transactions_length = 0;
    size_t fixed_length;
    size_t total_length;
    char *buffer;
    size_t offset;
    int written;

    if (block == NULL || output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    *output = NULL;

    /*
     * Both hashes must contain exactly 64 hexadecimal characters.
     */
    if (strlen(block->previous_hash) != SHA256_HEX_LENGTH ||
        strlen(block->merkle_root) != SHA256_HEX_LENGTH) {
        return ERR_INVALID_BLOCK;
    }

    if (block->transaction_count == 0 ||
        block->transactions == NULL) {
        return ERR_INVALID_BLOCK;
    }

    /*
     * Calculate the length of the transactions joined with "::".
     */
    for (size_t index = 0;
         index < block->transaction_count;
         index++) {
        if (block->transactions[index] == NULL) {
            return ERR_INVALID_TRANSACTION;
        }

        transactions_length += strlen(
            block->transactions[index]
        );

        if (index + 1 < block->transaction_count) {
            transactions_length += 2;
        }
    }

    /*
     * Fixed fields:
     *
     * index       = 16 hexadecimal characters
     * timestamp   = 16 hexadecimal characters
     * prev_hash   = 64 hexadecimal characters
     * merkle_root = 64 hexadecimal characters
     * nonce       = 16 hexadecimal characters
     */
    fixed_length =
        16 +
        16 +
        SHA256_HEX_LENGTH +
        SHA256_HEX_LENGTH +
        16;

    total_length = fixed_length + transactions_length;

    buffer = malloc(total_length + 1);

    if (buffer == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    /*
     * PRIx64 prints a uint64_t as hexadecimal.
     * 016 adds leading zeroes until the field has exactly 16 characters.
     */
    written = snprintf(
        buffer,
        total_length + 1,
        "%016" PRIx64
        "%016" PRIx64
        "%s"
        "%s"
        "%016" PRIx64,
        block->index,
        block->timestamp,
        block->previous_hash,
        block->merkle_root,
        block->nonce
    );

    if (written < 0 ||
        (size_t)written != fixed_length) {
        free(buffer);
        return ERR_SERIALIZATION;
    }

    offset = fixed_length;

    for (size_t index = 0;
         index < block->transaction_count;
         index++) {
        size_t transaction_length = strlen(
            block->transactions[index]
        );

        memcpy(
            buffer + offset,
            block->transactions[index],
            transaction_length
        );

        offset += transaction_length;

        if (index + 1 < block->transaction_count) {
            memcpy(buffer + offset, "::", 2);
            offset += 2;
        }
    }

    buffer[offset] = '\0';

    if (offset != total_length) {
        free(buffer);
        return ERR_SERIALIZATION;
    }

    *output = buffer;

    return PROJECT_OK;
}

int calculate_block_hash(
    const block_t *block,
    char output[SHA256_HEX_STRING_SIZE]
)
{
    char *hash_input;
    int result;

    if (block == NULL || output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    result = build_block_hash_input(
        block,
        &hash_input
    );

    if (result != PROJECT_OK) {
        return result;
    }

    result = sha256_hex(
        hash_input,
        strlen(hash_input),
        output
    );

    free(hash_input);

    return result;
}