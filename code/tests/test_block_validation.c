#include "block.h"
#include "crypto.h"
#include "errors.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

static int prepare_valid_block(block_t *block)
{
    int result;

    result = block_init(block);

    if (result != PROJECT_OK) {
        return result;
    }

    block->index = 1;
    block->timestamp = 1000;
    block->nonce = 42;

    strcpy(
        block->previous_hash,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    );

    result = block_add_transaction(
        block,
        "Alice pays Bob 10 coins"
    );

    if (result != PROJECT_OK) {
        block_destroy(block);
        return result;
    }

    result = block_add_transaction(
        block,
        "Bob pays Carol 5 coins"
    );

    if (result != PROJECT_OK) {
        block_destroy(block);
        return result;
    }

    result = calculate_merkle_root(
        (const char *const *)block->transactions,
        block->transaction_count,
        block->merkle_root
    );

    if (result != PROJECT_OK) {
        block_destroy(block);
        return result;
    }

    return PROJECT_OK;
}

static int test_valid_block_structure(void)
{
    block_t block;

    if (prepare_valid_block(&block) != PROJECT_OK) {
        return 1;
    }

    int result = validate_block_structure(&block);

    block_destroy(&block);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Valid block structure was rejected.\n");
        return 1;
    }

    printf("PASS: valid block structure\n");
    return 0;
}

static int test_invalid_transaction_in_block(void)
{
    block_t block;

    if (prepare_valid_block(&block) != PROJECT_OK) {
        return 1;
    }

    strcpy(
        block.transactions[0],
        "Alice give Bob 10 coins"
    );

    int result = validate_block_structure(&block);

    block_destroy(&block);

    if (result != ERR_INVALID_BLOCK) {
        fprintf(stderr, "Invalid transaction was not rejected.\n");
        return 1;
    }

    printf("PASS: invalid block transaction rejected\n");
    return 0;
}

static int test_valid_merkle_root(void)
{
    block_t block;

    if (prepare_valid_block(&block) != PROJECT_OK) {
        return 1;
    }

    int result = validate_block_merkle_root(&block);

    block_destroy(&block);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Valid Merkle root was rejected.\n");
        return 1;
    }

    printf("PASS: valid block Merkle root\n");
    return 0;
}

static int test_invalid_merkle_root(void)
{
    block_t block;

    if (prepare_valid_block(&block) != PROJECT_OK) {
        return 1;
    }

    block.merkle_root[0] =
        block.merkle_root[0] == 'a' ? 'b' : 'a';

    int result = validate_block_merkle_root(&block);

    block_destroy(&block);

    if (result != ERR_INVALID_BLOCK) {
        fprintf(stderr, "Incorrect Merkle root was not rejected.\n");
        return 1;
    }

    printf("PASS: incorrect Merkle root rejected\n");
    return 0;
}

static int test_null_block(void)
{
    if (validate_block_structure(NULL) != ERR_INVALID_ARGUMENT) {
        fprintf(stderr, "NULL structure block was not rejected.\n");
        return 1;
    }

    if (validate_block_merkle_root(NULL) != ERR_INVALID_ARGUMENT) {
        fprintf(stderr, "NULL Merkle block was not rejected.\n");
        return 1;
    }

    printf("PASS: NULL blocks rejected\n");
    return 0;
}

static int test_official_genesis_block(void)
{
    block_t genesis;
    int result;

    result = block_init(&genesis);

    if (result != PROJECT_OK) {
        return 1;
    }

    genesis.index = 0;
    genesis.timestamp = 0x67890001;
    genesis.nonce = 0;

    strcpy(
        genesis.previous_hash,
        "00000000000000000000000000000000"
        "00000000000000000000000000000000"
    );

    result = block_add_transaction(
        &genesis,
        "Genesis block"
    );

    if (result != PROJECT_OK) {
        block_destroy(&genesis);
        return 1;
    }

    result = calculate_merkle_root(
        (const char *const *)genesis.transactions,
        genesis.transaction_count,
        genesis.merkle_root
    );

    if (result != PROJECT_OK) {
        block_destroy(&genesis);
        return 1;
    }

    result = validate_block_structure(&genesis);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "Official genesis structure was rejected.\n"
        );

        block_destroy(&genesis);
        return 1;
    }

    result = validate_block_merkle_root(&genesis);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "Official genesis Merkle root was rejected.\n"
        );

        block_destroy(&genesis);
        return 1;
    }

    block_destroy(&genesis);

    printf("PASS: official genesis block validation\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_valid_block_structure();
    failed_tests += test_invalid_transaction_in_block();
    failed_tests += test_valid_merkle_root();
    failed_tests += test_invalid_merkle_root();
    failed_tests += test_null_block();
    failed_tests += test_official_genesis_block();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d block-validation test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All block-validation tests passed.\n");
    return 0;
}