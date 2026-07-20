#include "block.h"
#include "crypto.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prepare_test_block(block_t *block)
{
    int result;

    result = block_init(block);

    if (result != PROJECT_OK) {
        return result;
    }

    block->index = 4919;
    block->timestamp = 1000;
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

    return PROJECT_OK;
}

static int test_block_hash_input(void)
{
    block_t block;
    char *actual = NULL;

    const char *expected =
        "0000000000001337"
        "00000000000003e8"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "000000000000002a"
        "Alice pays Bob 10 coins"
        "::"
        "Bob pays Carol 5 coins";

    int result = prepare_test_block(&block);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Could not prepare test block.\n");
        return 1;
    }

    result = build_block_hash_input(
        &block,
        &actual
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "build_block_hash_input failed: %s\n",
            project_error_string(result)
        );

        block_destroy(&block);
        return 1;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "Block hash input differs.\n");
        fprintf(stderr, "Expected:\n%s\n", expected);
        fprintf(stderr, "Actual:\n%s\n", actual);

        free(actual);
        block_destroy(&block);
        return 1;
    }

    free(actual);
    block_destroy(&block);

    printf("PASS: canonical block hash input\n");

    return 0;
}

static int test_calculate_block_hash(void)
{
    block_t block;
    char *hash_input = NULL;
    char expected[SHA256_HEX_STRING_SIZE];
    char actual[SHA256_HEX_STRING_SIZE];

    int result = prepare_test_block(&block);

    if (result != PROJECT_OK) {
        return 1;
    }

    result = build_block_hash_input(
        &block,
        &hash_input
    );

    if (result != PROJECT_OK) {
        block_destroy(&block);
        return 1;
    }

    result = sha256_hex(
        hash_input,
        strlen(hash_input),
        expected
    );

    free(hash_input);

    if (result != PROJECT_OK) {
        block_destroy(&block);
        return 1;
    }

    result = calculate_block_hash(
        &block,
        actual
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "calculate_block_hash failed: %s\n",
            project_error_string(result)
        );

        block_destroy(&block);
        return 1;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "Calculated block hash differs.\n");
        fprintf(stderr, "Expected: %s\n", expected);
        fprintf(stderr, "Actual:   %s\n", actual);

        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: block hash calculation\n");

    return 0;
}

static int test_invalid_block_hash_input(void)
{
    block_t block;
    char *output = NULL;

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    /*
     * The hashes are empty after block_init(), so the block is invalid.
     */
    int result = build_block_hash_input(
        &block,
        &output
    );

    block_destroy(&block);

    if (result != ERR_INVALID_BLOCK) {
        fprintf(
            stderr,
            "Invalid block returned %d instead of %d.\n",
            result,
            ERR_INVALID_BLOCK
        );

        return 1;
    }

    printf("PASS: invalid block hash input rejected\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_block_hash_input();
    failed_tests += test_calculate_block_hash();
    failed_tests += test_invalid_block_hash_input();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d block-hash test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All block-hash tests passed.\n");

    return 0;
}