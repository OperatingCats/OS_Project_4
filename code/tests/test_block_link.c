#include "block.h"
#include "crypto.h"
#include "errors.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

static int prepare_block(
    block_t *block,
    uint64_t index,
    uint64_t timestamp,
    const char *previous_hash,
    const char *transaction
)
{
    int result;

    result = block_init(block);

    if (result != PROJECT_OK) {
        return result;
    }

    block->index = index;
    block->timestamp = timestamp;
    block->nonce = 42;

    strcpy(block->previous_hash, previous_hash);

    result = block_add_transaction(
        block,
        transaction
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

static int prepare_linked_blocks(
    block_t *previous,
    block_t *current
)
{
    char previous_hash[SHA256_HEX_STRING_SIZE];
    int result;

    result = prepare_block(
        previous,
        0,
        1000,
        "00000000000000000000000000000000"
        "00000000000000000000000000000000",
        "Genesis block"
    );

    if (result != PROJECT_OK) {
        return result;
    }

    result = calculate_block_hash(
        previous,
        previous_hash
    );

    if (result != PROJECT_OK) {
        block_destroy(previous);
        return result;
    }

    result = prepare_block(
        current,
        1,
        1100,
        previous_hash,
        "Bob pays Carol 5 coins"
    );

    if (result != PROJECT_OK) {
        block_destroy(previous);
        return result;
    }

    return PROJECT_OK;
}

static int test_valid_block_link(void)
{
    block_t previous;
    block_t current;

    if (prepare_linked_blocks(
            &previous,
            &current
        ) != PROJECT_OK) {
        return 1;
    }

    int result = validate_block_link(
        &previous,
        &current
    );

    block_destroy(&previous);
    block_destroy(&current);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Valid block link rejected.\n");
        return 1;
    }

    printf("PASS: valid block link\n");
    return 0;
}

static int test_invalid_previous_hash(void)
{
    block_t previous;
    block_t current;

    if (prepare_linked_blocks(
            &previous,
            &current
        ) != PROJECT_OK) {
        return 1;
    }

    current.previous_hash[0] =
        current.previous_hash[0] == 'a' ? 'b' : 'a';

    int result = validate_block_link(
        &previous,
        &current
    );

    block_destroy(&previous);
    block_destroy(&current);

    if (result != ERR_INVALID_BLOCKCHAIN) {
        fprintf(stderr, "Invalid previous hash accepted.\n");
        return 1;
    }

    printf("PASS: invalid previous hash rejected\n");
    return 0;
}

static int test_invalid_index(void)
{
    block_t previous;
    block_t current;

    if (prepare_linked_blocks(
            &previous,
            &current
        ) != PROJECT_OK) {
        return 1;
    }

    current.index = 5;

    int result = validate_block_link(
        &previous,
        &current
    );

    block_destroy(&previous);
    block_destroy(&current);

    if (result != ERR_INVALID_BLOCKCHAIN) {
        fprintf(stderr, "Invalid index accepted.\n");
        return 1;
    }

    printf("PASS: invalid index rejected\n");
    return 0;
}

static int test_invalid_timestamp(void)
{
    block_t previous;
    block_t current;

    if (prepare_linked_blocks(
            &previous,
            &current
        ) != PROJECT_OK) {
        return 1;
    }

    current.timestamp = previous.timestamp - 1;

    int result = validate_block_link(
        &previous,
        &current
    );

    block_destroy(&previous);
    block_destroy(&current);

    if (result != ERR_INVALID_BLOCKCHAIN) {
        fprintf(stderr, "Invalid timestamp accepted.\n");
        return 1;
    }

    printf("PASS: invalid timestamp rejected\n");
    return 0;
}

static int test_null_blocks(void)
{
    block_t block;

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    if (validate_block_link(NULL, &block) !=
        ERR_INVALID_ARGUMENT) {
        block_destroy(&block);
        return 1;
    }

    if (validate_block_link(&block, NULL) !=
        ERR_INVALID_ARGUMENT) {
        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: NULL block links rejected\n");
    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_valid_block_link();
    failed_tests += test_invalid_previous_hash();
    failed_tests += test_invalid_index();
    failed_tests += test_invalid_timestamp();
    failed_tests += test_null_blocks();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d block-link test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All block-link tests passed.\n");
    return 0;
}