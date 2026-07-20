#include "block.h"
#include "errors.h"

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

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_block_init();
    failed_tests += test_add_transaction();
    failed_tests += test_invalid_transaction();
    failed_tests += test_block_copy();

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