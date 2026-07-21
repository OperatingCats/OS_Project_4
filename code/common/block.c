#include "block.h"
#include "errors.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_hex_u64(
    const char *text,
    uint64_t *output
)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (strlen(text) != 16) {
        return ERR_FILE_FORMAT;
    }

    errno = 0;

    value = strtoull(text, &end, 16);

    if (errno != 0 ||
        end == text ||
        *end != '\0') {
        return ERR_FILE_FORMAT;
    }

    *output = (uint64_t)value;

    return PROJECT_OK;
}

static int is_lowercase_hex_hash(const char *hash)
{
    if (hash == NULL ||
        strlen(hash) != SHA256_HEX_LENGTH) {
        return 0;
    }

    for (size_t index = 0;
         index < SHA256_HEX_LENGTH;
         index++) {
        char character = hash[index];

        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return 0;
        }
    }

    return 1;
}

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

int block_to_csv(
    const block_t *block,
    char **output
)
{
    size_t transactions_length = 0;
    size_t total_length;
    size_t offset;
    char *line;
    int written;

    if (block == NULL || output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    *output = NULL;

    if (!is_lowercase_hex_hash(block->previous_hash) ||
        !is_lowercase_hex_hash(block->merkle_root)) {
        return ERR_INVALID_BLOCK;
    }

    if (block->transactions == NULL ||
        block->transaction_count == 0 ||
        block->transaction_count >
            MAX_TRANSACTIONS_PER_BLOCK) {
        return ERR_INVALID_BLOCK;
    }

    for (size_t index = 0;
         index < block->transaction_count;
         index++) {
        size_t transaction_length;

        if (block->transactions[index] == NULL) {
            return ERR_INVALID_TRANSACTION;
        }

        transaction_length =
            strlen(block->transactions[index]);

        if (transaction_length == 0 ||
            transaction_length >=
                MAX_TRANSACTION_LENGTH) {
            return ERR_INVALID_TRANSACTION;
        }

        /*
         * Transactions cannot contain characters that would
         * conflict with this CSV representation.
         */
        if (strchr(block->transactions[index], '"') != NULL ||
            strstr(block->transactions[index], "::") != NULL ||
            strchr(block->transactions[index], '\n') != NULL ||
            strchr(block->transactions[index], '\r') != NULL) {
            return ERR_INVALID_TRANSACTION;
        }

        transactions_length += transaction_length;

        if (index + 1 < block->transaction_count) {
            transactions_length += 2;
        }
    }

    /*
     * Fixed part:
     * 16 + comma
     * 16 + comma
     * 64 + comma
     * 64 + comma
     * 16 + comma
     * opening quote + closing quote
     */
    total_length =
        16 + 1 +
        16 + 1 +
        SHA256_HEX_LENGTH + 1 +
        SHA256_HEX_LENGTH + 1 +
        16 + 1 +
        2 +
        transactions_length;

    line = malloc(total_length + 1);

    if (line == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    written = snprintf(
        line,
        total_length + 1,
        "%016" PRIx64 ","
        "%016" PRIx64 ","
        "%s,"
        "%s,"
        "%016" PRIx64 ",\"",
        block->index,
        block->timestamp,
        block->previous_hash,
        block->merkle_root,
        block->nonce
    );

    if (written < 0 ||
        (size_t)written > total_length) {
        free(line);
        return ERR_SERIALIZATION;
    }

    offset = (size_t)written;

    for (size_t index = 0;
         index < block->transaction_count;
         index++) {
        size_t transaction_length =
            strlen(block->transactions[index]);

        memcpy(
            line + offset,
            block->transactions[index],
            transaction_length
        );

        offset += transaction_length;

        if (index + 1 < block->transaction_count) {
            memcpy(line + offset, "::", 2);
            offset += 2;
        }
    }

    line[offset++] = '"';
    line[offset] = '\0';

    if (offset != total_length) {
        free(line);
        return ERR_SERIALIZATION;
    }

    *output = line;

    return PROJECT_OK;
}

int block_from_csv(
    const char *line,
    block_t *output
)
{
    block_t parsed;
    char *copy;
    char *fields[6];
    char *cursor;
    size_t field_count = 0;
    int result;

    if (line == NULL || output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    copy = malloc(strlen(line) + 1);

    if (copy == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    strcpy(copy, line);

    /*
     * Remove a possible newline read by fgets().
     */
    copy[strcspn(copy, "\r\n")] = '\0';

    cursor = copy;

    /*
     * The first five fields cannot contain commas.
     */
    while (field_count < 5) {
        char *comma = strchr(cursor, ',');

        if (comma == NULL) {
            free(copy);
            return ERR_FILE_FORMAT;
        }

        *comma = '\0';
        fields[field_count++] = cursor;
        cursor = comma + 1;
    }

    fields[field_count++] = cursor;

    if (fields[5][0] != '"') {
        free(copy);
        return ERR_FILE_FORMAT;
    }

    size_t transactions_field_length =
        strlen(fields[5]);

    if (transactions_field_length < 2 ||
        fields[5][transactions_field_length - 1] != '"') {
        free(copy);
        return ERR_FILE_FORMAT;
    }

    fields[5][transactions_field_length - 1] = '\0';
    fields[5]++;

    result = block_init(&parsed);

    if (result != PROJECT_OK) {
        free(copy);
        return result;
    }

    result = parse_hex_u64(fields[0], &parsed.index);

    if (result == PROJECT_OK) {
        result = parse_hex_u64(
            fields[1],
            &parsed.timestamp
        );
    }

    if (result == PROJECT_OK &&
        !is_lowercase_hex_hash(fields[2])) {
        result = ERR_FILE_FORMAT;
    }

    if (result == PROJECT_OK &&
        !is_lowercase_hex_hash(fields[3])) {
        result = ERR_FILE_FORMAT;
    }

    if (result == PROJECT_OK) {
        strcpy(parsed.previous_hash, fields[2]);
        strcpy(parsed.merkle_root, fields[3]);

        result = parse_hex_u64(
            fields[4],
            &parsed.nonce
        );
    }

    if (result == PROJECT_OK) {
        char *transaction_cursor = fields[5];

        if (*transaction_cursor == '\0') {
            result = ERR_FILE_FORMAT;
        }

        while (result == PROJECT_OK &&
               transaction_cursor != NULL) {
            char *separator =
                strstr(transaction_cursor, "::");

            if (separator != NULL) {
                *separator = '\0';
            }

            result = block_add_transaction(
                &parsed,
                transaction_cursor
            );

            if (separator == NULL) {
                transaction_cursor = NULL;
            } else {
                transaction_cursor = separator + 2;
            }
        }
    }

    free(copy);

    if (result != PROJECT_OK) {
        block_destroy(&parsed);
        return result;
    }

    /*
     * Project convention: output must previously have been
     * initialized with block_init().
     */
    block_destroy(output);
    *output = parsed;

    return PROJECT_OK;
}

int block_serialize(
    const block_t *block,
    unsigned char **buffer,
    size_t *buffer_size
)
{
    char *csv_line;
    size_t length;
    int result;

    if (block == NULL ||
        buffer == NULL ||
        buffer_size == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    *buffer = NULL;
    *buffer_size = 0;

    result = block_to_csv(block, &csv_line);

    if (result != PROJECT_OK) {
        return result;
    }

    /*
     * The protocol sends raw payload bytes. The final '\0'
     * is not included in payload_len.
     */
    length = strlen(csv_line);

    *buffer = malloc(length);

    if (*buffer == NULL) {
        free(csv_line);
        return ERR_MEMORY_ALLOCATION;
    }

    memcpy(*buffer, csv_line, length);

    *buffer_size = length;

    free(csv_line);

    return PROJECT_OK;
}

int block_deserialize(
    const unsigned char *buffer,
    size_t buffer_size,
    block_t *output
)
{
    char *csv_line;
    int result;

    if (buffer == NULL ||
        buffer_size == 0 ||
        output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (memchr(buffer, '\0', buffer_size) != NULL) {
        return ERR_SERIALIZATION;
    }

    csv_line = malloc(buffer_size + 1);

    if (csv_line == NULL) {
        return ERR_MEMORY_ALLOCATION;
    }

    memcpy(csv_line, buffer, buffer_size);
    csv_line[buffer_size] = '\0';

    result = block_from_csv(csv_line, output);

    free(csv_line);

    return result;
}