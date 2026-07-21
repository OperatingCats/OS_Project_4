#include "blockchain.h"

#include "crypto.h"
#include "errors.h"

#include <stdio.h>
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

#define BLOCKCHAIN_CSV_HEADER \
    "index,timestamp,prev_hash,merkle_root,nonce,transactions"

#define MAX_BLOCK_CSV_LINE_LENGTH 8192

int blockchain_save_csv(
    const blockchain_t *chain,
    const char *path
)
{
    FILE *file;

    if (chain == NULL || path == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (chain->count > 0 && chain->blocks == NULL) {
        return ERR_INVALID_BLOCKCHAIN;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return ERR_FILE_OPEN;
    }

    if (fprintf(file, "%s\n", BLOCKCHAIN_CSV_HEADER) < 0) {
        fclose(file);
        return ERR_FILE_WRITE;
    }

    for (size_t index = 0; index < chain->count; index++) {
        char *csv_line = NULL;

        int result = block_to_csv(
            &chain->blocks[index],
            &csv_line
        );

        if (result != PROJECT_OK) {
            fclose(file);
            return result;
        }

        if (fprintf(file, "%s\n", csv_line) < 0) {
            free(csv_line);
            fclose(file);
            return ERR_FILE_WRITE;
        }

        free(csv_line);
    }

    if (fclose(file) != 0) {
        return ERR_FILE_WRITE;
    }

    return PROJECT_OK;
}

int blockchain_load_csv(
    blockchain_t *chain,
    const char *path
)
{
    FILE *file;
    blockchain_t loaded_chain;
    char line[MAX_BLOCK_CSV_LINE_LENGTH];
    int result;

    if (chain == NULL || path == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return ERR_FILE_OPEN;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return ERR_FILE_READ;
    }

    /*
     * Reject a header longer than the available buffer.
     */
    if (strchr(line, '\n') == NULL && !feof(file)) {
        fclose(file);
        return ERR_FILE_FORMAT;
    }

    line[strcspn(line, "\r\n")] = '\0';

    if (strcmp(line, BLOCKCHAIN_CSV_HEADER) != 0) {
        fclose(file);
        return ERR_FILE_FORMAT;
    }

    result = blockchain_init(&loaded_chain);

    if (result != PROJECT_OK) {
        fclose(file);
        return result;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        block_t block;

        /*
         * Reject lines that exceed the buffer.
         */
        if (strchr(line, '\n') == NULL && !feof(file)) {
            blockchain_destroy(&loaded_chain);
            fclose(file);
            return ERR_FILE_FORMAT;
        }

        line[strcspn(line, "\r\n")] = '\0';

        /*
         * Ignore empty lines.
         */
        if (line[0] == '\0') {
            continue;
        }

        result = block_init(&block);

        if (result != PROJECT_OK) {
            blockchain_destroy(&loaded_chain);
            fclose(file);
            return result;
        }

        result = block_from_csv(line, &block);

        if (result == PROJECT_OK) {
            result = blockchain_append(
                &loaded_chain,
                &block
            );
        }

        block_destroy(&block);

        if (result != PROJECT_OK) {
            blockchain_destroy(&loaded_chain);
            fclose(file);
            return result;
        }
    }

    if (ferror(file)) {
        blockchain_destroy(&loaded_chain);
        fclose(file);
        return ERR_FILE_READ;
    }

    if (fclose(file) != 0) {
        blockchain_destroy(&loaded_chain);
        return ERR_FILE_READ;
    }

    /*
     * A valid blockchain state must contain at least the genesis block.
     */
    if (loaded_chain.count == 0) {
        blockchain_destroy(&loaded_chain);
        return ERR_FILE_FORMAT;
    }

    /*
     * The destination chain must already have been initialized.
     * Replace it only after the complete file has been loaded.
     */
    blockchain_destroy(chain);
    *chain = loaded_chain;

    return PROJECT_OK;
}