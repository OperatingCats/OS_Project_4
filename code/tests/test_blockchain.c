#include "block.h"
#include "blockchain.h"
#include "crypto.h"
#include "errors.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

static int prepare_block(
    block_t *block,
    uint64_t index,
    const char *transaction
)
{
    int result;

    result = block_init(block);

    if (result != PROJECT_OK) {
        return result;
    }

    block->index = index;
    block->timestamp = 1000 + index;
    block->nonce = index;

    strcpy(
        block->previous_hash,
        "00000000000000000000000000000000"
        "00000000000000000000000000000000"
    );

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

static int test_blockchain_init(void)
{
    blockchain_t chain;

    if (blockchain_init(&chain) != PROJECT_OK) {
        fprintf(stderr, "blockchain_init failed.\n");
        return 1;
    }

    if (chain.blocks != NULL ||
        chain.count != 0 ||
        chain.capacity != 0) {
        fprintf(stderr, "Invalid initial blockchain state.\n");
        blockchain_destroy(&chain);
        return 1;
    }

    blockchain_destroy(&chain);

    printf("PASS: blockchain_init\n");
    return 0;
}

static int test_blockchain_append(void)
{
    blockchain_t chain;
    block_t block;

    if (blockchain_init(&chain) != PROJECT_OK) {
        return 1;
    }

    if (prepare_block(
            &block,
            0,
            "Alice pays Bob 10 coins"
        ) != PROJECT_OK) {
        blockchain_destroy(&chain);
        return 1;
    }

    int result = blockchain_append(&chain, &block);

    if (result != PROJECT_OK) {
        fprintf(stderr, "blockchain_append failed.\n");
        block_destroy(&block);
        blockchain_destroy(&chain);
        return 1;
    }

    if (chain.count != 1) {
        fprintf(stderr, "Blockchain count is incorrect.\n");
        block_destroy(&block);
        blockchain_destroy(&chain);
        return 1;
    }

    /*
     * Verify that blockchain_append made a deep copy.
     */
    block.transactions[0][0] = 'X';

    if (chain.blocks[0].transactions[0][0] == 'X') {
        fprintf(stderr, "Block was not copied deeply.\n");
        block_destroy(&block);
        blockchain_destroy(&chain);
        return 1;
    }

    block_destroy(&block);
    blockchain_destroy(&chain);

    printf("PASS: blockchain_append\n");
    return 0;
}

static int test_get_by_index(void)
{
    blockchain_t chain;
    block_t first;
    block_t second;

    if (blockchain_init(&chain) != PROJECT_OK) {
        return 1;
    }

    if (prepare_block(
            &first,
            10,
            "Alice pays Bob 10 coins"
        ) != PROJECT_OK) {
        blockchain_destroy(&chain);
        return 1;
    }

    if (prepare_block(
            &second,
            20,
            "Bob pays Carol 5 coins"
        ) != PROJECT_OK) {
        block_destroy(&first);
        blockchain_destroy(&chain);
        return 1;
    }

    if (blockchain_append(&chain, &first) != PROJECT_OK ||
        blockchain_append(&chain, &second) != PROJECT_OK) {
        block_destroy(&first);
        block_destroy(&second);
        blockchain_destroy(&chain);
        return 1;
    }

    const block_t *found = blockchain_get_by_index(
        &chain,
        20
    );

    if (found == NULL || found->index != 20) {
        fprintf(stderr, "Block not found by index.\n");
        block_destroy(&first);
        block_destroy(&second);
        blockchain_destroy(&chain);
        return 1;
    }

    if (blockchain_get_by_index(&chain, 999) != NULL) {
        fprintf(stderr, "Nonexistent block was found.\n");
        block_destroy(&first);
        block_destroy(&second);
        blockchain_destroy(&chain);
        return 1;
    }

    block_destroy(&first);
    block_destroy(&second);
    blockchain_destroy(&chain);

    printf("PASS: blockchain_get_by_index\n");
    return 0;
}

static int test_get_by_hash(void)
{
    blockchain_t chain;
    block_t block;
    char hash[SHA256_HEX_STRING_SIZE];

    if (blockchain_init(&chain) != PROJECT_OK) {
        return 1;
    }

    if (prepare_block(
            &block,
            5,
            "Alice pays Bob 10 coins"
        ) != PROJECT_OK) {
        blockchain_destroy(&chain);
        return 1;
    }

    if (calculate_block_hash(&block, hash) != PROJECT_OK) {
        block_destroy(&block);
        blockchain_destroy(&chain);
        return 1;
    }

    if (blockchain_append(&chain, &block) != PROJECT_OK) {
        block_destroy(&block);
        blockchain_destroy(&chain);
        return 1;
    }

    const block_t *found = blockchain_get_by_hash(
        &chain,
        hash
    );

    if (found == NULL || found->index != 5) {
        fprintf(stderr, "Block not found by hash.\n");
        block_destroy(&block);
        blockchain_destroy(&chain);
        return 1;
    }

    block_destroy(&block);
    blockchain_destroy(&chain);

    printf("PASS: blockchain_get_by_hash\n");
    return 0;
}

static int test_invalid_arguments(void)
{
    block_t block;

    if (block_init(&block) != PROJECT_OK) {
        return 1;
    }

    if (blockchain_init(NULL) != ERR_INVALID_ARGUMENT ||
        blockchain_append(NULL, &block) != ERR_INVALID_ARGUMENT ||
        blockchain_append(NULL, NULL) != ERR_INVALID_ARGUMENT ||
        blockchain_get_by_index(NULL, 0) != NULL ||
        blockchain_get_by_hash(NULL, "hash") != NULL) {
        fprintf(stderr, "Invalid arguments not handled correctly.\n");
        block_destroy(&block);
        return 1;
    }

    block_destroy(&block);

    printf("PASS: blockchain invalid arguments\n");
    return 0;
}

static int prepare_linked_chain(blockchain_t *chain)
{
    block_t first;
    block_t second;
    char first_hash[SHA256_HEX_STRING_SIZE];
    int result;

    result = blockchain_init(chain);

    if (result != PROJECT_OK) {
        return result;
    }

    result = prepare_block(
        &first,
        0,
        "Alice pays Bob 10 coins"
    );

    if (result != PROJECT_OK) {
        blockchain_destroy(chain);
        return result;
    }

    result = calculate_block_hash(
        &first,
        first_hash
    );

    if (result != PROJECT_OK) {
        block_destroy(&first);
        blockchain_destroy(chain);
        return result;
    }

    result = prepare_block(
        &second,
        1,
        "Bob pays Carol 5 coins"
    );

    if (result != PROJECT_OK) {
        block_destroy(&first);
        blockchain_destroy(chain);
        return result;
    }

    second.timestamp = first.timestamp + 1;

    strcpy(
        second.previous_hash,
        first_hash
    );

    result = blockchain_append(chain, &first);

    if (result == PROJECT_OK) {
        result = blockchain_append(chain, &second);
    }

    block_destroy(&first);
    block_destroy(&second);

    if (result != PROJECT_OK) {
        blockchain_destroy(chain);
        return result;
    }

    return PROJECT_OK;
}

static int test_valid_blockchain(void)
{
    blockchain_t chain;

    if (prepare_linked_chain(&chain) != PROJECT_OK) {
        return 1;
    }

    int result = validate_blockchain(&chain);

    blockchain_destroy(&chain);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Valid blockchain was rejected.\n");
        return 1;
    }

    printf("PASS: valid blockchain\n");
    return 0;
}

static int test_invalid_blockchain_link(void)
{
    blockchain_t chain;

    if (prepare_linked_chain(&chain) != PROJECT_OK) {
        return 1;
    }

    chain.blocks[1].previous_hash[0] =
        chain.blocks[1].previous_hash[0] == 'a' ? 'b' : 'a';

    int result = validate_blockchain(&chain);

    blockchain_destroy(&chain);

    if (result != ERR_INVALID_BLOCKCHAIN) {
        fprintf(stderr, "Invalid blockchain link was accepted.\n");
        return 1;
    }

    printf("PASS: invalid blockchain link rejected\n");
    return 0;
}

static int test_invalid_blockchain_merkle_root(void)
{
    blockchain_t chain;

    if (prepare_linked_chain(&chain) != PROJECT_OK) {
        return 1;
    }

    chain.blocks[0].merkle_root[0] =
        chain.blocks[0].merkle_root[0] == 'a' ? 'b' : 'a';

    int result = validate_blockchain(&chain);

    blockchain_destroy(&chain);

    if (result != ERR_INVALID_BLOCKCHAIN) {
        fprintf(stderr, "Invalid blockchain Merkle root was accepted.\n");
        return 1;
    }

    printf("PASS: invalid blockchain Merkle root rejected\n");
    return 0;
}

static int test_empty_blockchain_validation(void)
{
    blockchain_t chain;

    if (blockchain_init(&chain) != PROJECT_OK) {
        return 1;
    }

    int result = validate_blockchain(&chain);

    blockchain_destroy(&chain);

    if (result != ERR_INVALID_BLOCKCHAIN) {
        fprintf(stderr, "Empty blockchain was accepted.\n");
        return 1;
    }

    printf("PASS: empty blockchain rejected\n");
    return 0;
}

static int test_null_blockchain_validation(void)
{
    if (validate_blockchain(NULL) != ERR_INVALID_ARGUMENT) {
        fprintf(stderr, "NULL blockchain was not rejected.\n");
        return 1;
    }

    printf("PASS: NULL blockchain rejected\n");
    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_blockchain_init();
    failed_tests += test_blockchain_append();
    failed_tests += test_get_by_index();
    failed_tests += test_get_by_hash();
    failed_tests += test_invalid_arguments();
    failed_tests += test_valid_blockchain();
    failed_tests += test_invalid_blockchain_link();
    failed_tests += test_invalid_blockchain_merkle_root();
    failed_tests += test_empty_blockchain_validation();
    failed_tests += test_null_blockchain_validation();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d blockchain test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All blockchain tests passed.\n");
    return 0;
}
