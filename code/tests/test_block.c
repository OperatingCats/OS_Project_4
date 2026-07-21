#include "block.h"
#include "errors.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int test_block_init(void)
{
    block_t block;

    int result = block_init(&block);

    if (result != PROJECT_OK) {
        fprintf(stderr, "block_init failed.\n");
        return 1;
    }

    if (block.index != 0 ||
        block.timestamp != 0 ||
        block.nonce != 0 ||
        block.transaction_count != 0 ||
        block.transactions != NULL ||
        block.previous_hash[0] != '\0' ||
        block.merkle_root[0] != '\0') {
        fprintf(stderr, "block_init produced invalid state.\n");
        return 1;
    }

    block_destroy(&block);

    printf("PASS: block_init\n");

    return 0;
}

static int test_add_transaction(void)
{
    block_t block;
    const char *transaction = "Alice pays Bob 10 coins";

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    int result = block_add_transaction(
        &block,
        transaction
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "block_add_transaction failed: %d\n",
            result
        );

        block_destroy(&block);
        return 1;
    }

    if (block.transaction_count != 1) {
        fprintf(stderr, "Wrong transaction count.\n");
        block_destroy(&block);
        return 1;
    }

    if (strcmp(block.transactions[0], transaction) != 0) {
        fprintf(stderr, "Stored transaction is incorrect.\n");
        block_destroy(&block);
        return 1;
    }

    if (block.transactions[0] == transaction) {
        fprintf(stderr, "Transaction was not deep copied.\n");
        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: block_add_transaction\n");

    return 0;
}

static int test_invalid_transaction(void)
{
    block_t block;

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    int result = block_add_transaction(&block, "");

    if (result != ERR_INVALID_TRANSACTION) {
        fprintf(
            stderr,
            "Empty transaction should be rejected.\n"
        );

        block_destroy(&block);
        return 1;
    }

    result = block_add_transaction(&block, NULL);

    if (result != ERR_INVALID_ARGUMENT) {
        fprintf(
            stderr,
            "NULL transaction should be rejected.\n"
        );

        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: invalid transaction arguments\n");

    return 0;
}

static int test_block_copy(void)
{
    block_t source;
    block_t destination;

    if (block_init(&source) != PROJECT_OK ||
        block_init(&destination) != PROJECT_OK) {
        return 1;
    }

    source.index = 7;
    source.timestamp = 123456789;
    source.nonce = 42;

    strcpy(
        source.previous_hash,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    );

    strcpy(
        source.merkle_root,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    );

    if (block_add_transaction(
            &source,
            "Alice pays Bob 10 coins"
        ) != PROJECT_OK ||
        block_add_transaction(
            &source,
            "Bob pays Carol 5 coins"
        ) != PROJECT_OK) {
        block_destroy(&source);
        block_destroy(&destination);
        return 1;
    }

    int result = block_copy(
        &destination,
        &source
    );

    if (result != PROJECT_OK) {
        fprintf(stderr, "block_copy failed.\n");
        block_destroy(&source);
        block_destroy(&destination);
        return 1;
    }

    if (destination.index != source.index ||
        destination.timestamp != source.timestamp ||
        destination.nonce != source.nonce ||
        strcmp(
            destination.previous_hash,
            source.previous_hash
        ) != 0 ||
        strcmp(
            destination.merkle_root,
            source.merkle_root
        ) != 0 ||
        destination.transaction_count !=
            source.transaction_count) {
        fprintf(stderr, "Copied block fields differ.\n");

        block_destroy(&source);
        block_destroy(&destination);
        return 1;
    }

    if (destination.transactions[0] ==
        source.transactions[0]) {
        fprintf(stderr, "Block copy is not deep.\n");

        block_destroy(&source);
        block_destroy(&destination);
        return 1;
    }

    source.transactions[0][0] = 'X';

    if (strcmp(
            destination.transactions[0],
            "Alice pays Bob 10 coins"
        ) != 0) {
        fprintf(stderr, "Copied block depends on source memory.\n");

        block_destroy(&source);
        block_destroy(&destination);
        return 1;
    }

    block_destroy(&source);
    block_destroy(&destination);

    printf("PASS: block_copy\n");

    return 0;
}

static int prepare_serialization_block(block_t *block)
{
    if (block_init(block) != PROJECT_OK) {
        return 1;
    }

    block->index = 7;
    block->timestamp = 123456789;
    block->nonce = 42;

    strcpy(
        block->previous_hash,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    );

    strcpy(
        block->merkle_root,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    );

    if (block_add_transaction(
            block,
            "Alice pays Bob 10 coins"
        ) != PROJECT_OK ||
        block_add_transaction(
            block,
            "Bob pays Carol 5 coins"
        ) != PROJECT_OK) {
        block_destroy(block);
        return 1;
    }

    return 0;
}

static int blocks_are_equal(
    const block_t *first,
    const block_t *second
)
{
    if (first->index != second->index ||
        first->timestamp != second->timestamp ||
        first->nonce != second->nonce ||
        strcmp(first->previous_hash, second->previous_hash) != 0 ||
        strcmp(first->merkle_root, second->merkle_root) != 0 ||
        first->transaction_count != second->transaction_count) {
        return 0;
    }

    for (size_t index = 0;
         index < first->transaction_count;
         index++) {
        if (strcmp(
                first->transactions[index],
                second->transactions[index]
            ) != 0) {
            return 0;
        }
    }

    return 1;
}

static int test_block_csv_round_trip(void)
{
    block_t original;
    block_t restored;
    char *csv = NULL;
    int result;

    if (prepare_serialization_block(&original) != 0) {
        return 1;
    }

    if (block_init(&restored) != PROJECT_OK) {
        block_destroy(&original);
        return 1;
    }

    result = block_to_csv(&original, &csv);

    if (result != PROJECT_OK) {
        fprintf(stderr, "block_to_csv failed: %d\n", result);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    result = block_from_csv(csv, &restored);

    if (result != PROJECT_OK) {
        fprintf(stderr, "block_from_csv failed: %d\n", result);
        free(csv);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    if (!blocks_are_equal(&original, &restored)) {
        fprintf(stderr, "CSV round trip changed the block.\n");
        free(csv);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    free(csv);
    block_destroy(&original);
    block_destroy(&restored);

    printf("PASS: block CSV round trip\n");
    return 0;
}

static int test_block_serialization_round_trip(void)
{
    block_t original;
    block_t restored;
    unsigned char *buffer = NULL;
    size_t buffer_size = 0;
    int result;

    if (prepare_serialization_block(&original) != 0) {
        return 1;
    }

    if (block_init(&restored) != PROJECT_OK) {
        block_destroy(&original);
        return 1;
    }

    result = block_serialize(
        &original,
        &buffer,
        &buffer_size
    );

    if (result != PROJECT_OK) {
        fprintf(stderr, "block_serialize failed: %d\n", result);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    if (buffer == NULL || buffer_size == 0) {
        fprintf(stderr, "Serialized buffer is empty.\n");
        free(buffer);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    result = block_deserialize(
        buffer,
        buffer_size,
        &restored
    );

    if (result != PROJECT_OK) {
        fprintf(stderr, "block_deserialize failed: %d\n", result);
        free(buffer);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    if (!blocks_are_equal(&original, &restored)) {
        fprintf(stderr, "Serialization round trip changed the block.\n");
        free(buffer);
        block_destroy(&original);
        block_destroy(&restored);
        return 1;
    }

    free(buffer);
    block_destroy(&original);
    block_destroy(&restored);

    printf("PASS: block serialization round trip\n");
    return 0;
}

static int test_genesis_csv(void)
{
    const char *line =
        "0000000000000000,"
        "0000000067890001,"
        "00000000000000000000000000000000"
        "00000000000000000000000000000000,"
        "b815a93dd7f59058a27e63558ba5aa64"
        "45d851f316070ec13db673d5ab38e0cc,"
        "0000000000000000,"
        "\"Genesis block\"";

    block_t block;

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    int result = block_from_csv(line, &block);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Genesis CSV was rejected: %d\n", result);
        block_destroy(&block);
        return 1;
    }

    if (block.index != 0 ||
        block.timestamp != UINT64_C(0x67890001) ||
        block.nonce != 0 ||
        block.transaction_count != 1 ||
        strcmp(block.transactions[0], "Genesis block") != 0) {
        fprintf(stderr, "Genesis CSV was parsed incorrectly.\n");
        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: genesis CSV parsing\n");
    return 0;
}

static int test_invalid_csv(void)
{
    block_t block;

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    const char *missing_quotes =
        "0000000000000000,"
        "0000000000000001,"
        "00000000000000000000000000000000"
        "00000000000000000000000000000000,"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa,"
        "0000000000000000,"
        "Genesis block";

    int result = block_from_csv(
        missing_quotes,
        &block
    );

    if (result != ERR_FILE_FORMAT) {
        fprintf(stderr, "CSV without quotes was accepted.\n");
        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: invalid CSV rejected\n");
    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_block_init();
    failed_tests += test_add_transaction();
    failed_tests += test_invalid_transaction();
    failed_tests += test_block_copy();

    failed_tests += test_block_csv_round_trip();
    failed_tests += test_block_serialization_round_trip();
    failed_tests += test_genesis_csv();
    failed_tests += test_invalid_csv();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d block test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All block tests passed.\n");

    return 0;
}