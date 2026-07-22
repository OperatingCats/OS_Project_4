#include "miner.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

static int contains(
    char out[][MAX_TRANSACTION_LENGTH],
    size_t out_count,
    const char *value
)
{
    for (size_t index = 0; index < out_count; index++) {
        if (strcmp(out[index], value) == 0) {
            return 1;
        }
    }

    return 0;
}

static int test_mempool_init_and_destroy(void)
{
    mempool_t pool;

    if (mempool_init(&pool) != PROJECT_OK) {
        fprintf(stderr, "mempool_init failed.\n");
        return 1;
    }

    if (mempool_size(&pool) != 0) {
        fprintf(stderr, "Freshly initialized mempool is not empty.\n");
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_init_and_destroy\n");

    return 0;
}

static int test_mempool_add(void)
{
    mempool_t pool;
    char snapshot[4][MAX_TRANSACTION_LENGTH];
    size_t copied;

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    if (mempool_add(&pool, "Alice pays Bob 10 coins") != PROJECT_OK) {
        fprintf(stderr, "mempool_add rejected a valid transaction.\n");
        mempool_destroy(&pool);
        return 1;
    }

    if (mempool_size(&pool) != 1) {
        fprintf(stderr, "mempool_size did not reflect the added transaction.\n");
        mempool_destroy(&pool);
        return 1;
    }

    copied = mempool_snapshot(&pool, snapshot, 4);

    if (copied != 1 ||
        strcmp(snapshot[0], "Alice pays Bob 10 coins") != 0) {
        fprintf(stderr, "mempool_snapshot did not return the added transaction.\n");
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_add\n");

    return 0;
}

static int test_mempool_add_rejects_invalid(void)
{
    mempool_t pool;
    char too_long[MAX_TRANSACTION_LENGTH + 1];

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    memset(too_long, 'A', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';

    if (mempool_add(&pool, too_long) != ERR_INVALID_TRANSACTION) {
        fprintf(stderr, "mempool_add accepted an over-length transaction.\n");
        mempool_destroy(&pool);
        return 1;
    }

    if (mempool_add(&pool, "") != ERR_INVALID_TRANSACTION) {
        fprintf(stderr, "mempool_add accepted an empty transaction.\n");
        mempool_destroy(&pool);
        return 1;
    }

    if (mempool_add(NULL, "Alice pays Bob 10 coins") != ERR_INVALID_ARGUMENT ||
        mempool_add(&pool, NULL) != ERR_INVALID_ARGUMENT) {
        fprintf(stderr, "mempool_add did not reject NULL arguments.\n");
        mempool_destroy(&pool);
        return 1;
    }

    if (mempool_size(&pool) != 0) {
        fprintf(stderr, "A rejected transaction was still stored.\n");
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_add_rejects_invalid\n");

    return 0;
}

static int test_mempool_add_respects_capacity(void)
{
    mempool_t pool;
    char transaction[MAX_TRANSACTION_LENGTH];

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    for (size_t index = 0; index < MEMPOOL_CAPACITY; index++) {
        snprintf(
            transaction,
            sizeof(transaction),
            "Alice pays Bob %zu coins",
            index + 1
        );

        if (mempool_add(&pool, transaction) != PROJECT_OK) {
            fprintf(
                stderr,
                "mempool_add failed before reaching capacity (at %zu).\n",
                index
            );
            mempool_destroy(&pool);
            return 1;
        }
    }

    if (mempool_size(&pool) != MEMPOOL_CAPACITY) {
        fprintf(stderr, "mempool_size does not match MEMPOOL_CAPACITY after filling.\n");
        mempool_destroy(&pool);
        return 1;
    }

    if (mempool_add(&pool, "Alice pays Bob 999 coins") != ERR_INVALID_BLOCK) {
        fprintf(stderr, "mempool_add did not reject an add past capacity.\n");
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_add_respects_capacity\n");

    return 0;
}

static int test_mempool_snapshot_respects_max_count(void)
{
    mempool_t pool;
    char snapshot[2][MAX_TRANSACTION_LENGTH];
    size_t copied;

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    mempool_add(&pool, "Alice pays Bob 1 coins");
    mempool_add(&pool, "Bob pays Charlie 2 coins");
    mempool_add(&pool, "Charlie pays Dave 3 coins");

    copied = mempool_snapshot(&pool, snapshot, 2);

    if (copied != 2) {
        fprintf(
            stderr,
            "mempool_snapshot copied %zu entries, expected the requested max of 2.\n",
            copied
        );
        mempool_destroy(&pool);
        return 1;
    }

    if (mempool_size(&pool) != 3) {
        fprintf(stderr, "mempool_snapshot must not remove entries from the pool.\n");
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_snapshot_respects_max_count\n");

    return 0;
}

/*
 * mempool_remove_included() removes matching entries by swapping in the
 * current last element and re-checking that same slot, without advancing
 * past it. This is the classic pattern that silently skips an element if
 * implemented off by one, so this test removes two *adjacent* entries
 * (one of which is the actual last element) to force a swap-into a slot
 * that must itself also be removed.
 */
static int test_mempool_remove_included_adjacent(void)
{
    mempool_t pool;
    char snapshot[8][MAX_TRANSACTION_LENGTH];
    size_t copied;
    char *included[2];

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    mempool_add(&pool, "A pays X 1 coins");
    mempool_add(&pool, "B pays X 2 coins");
    mempool_add(&pool, "C pays X 3 coins");
    mempool_add(&pool, "D pays X 4 coins");
    mempool_add(&pool, "E pays X 5 coins");

    /* B is not the last element; E is. Removing both forces the pool to
     * swap E into B's slot, and that swapped-in E must also be removed
     * in the same pass. */
    included[0] = "B pays X 2 coins";
    included[1] = "E pays X 5 coins";

    mempool_remove_included(&pool, included, 2);

    if (mempool_size(&pool) != 3) {
        fprintf(
            stderr,
            "Expected 3 transactions left after removing 2 of 5, got %zu.\n",
            mempool_size(&pool)
        );
        mempool_destroy(&pool);
        return 1;
    }

    copied = mempool_snapshot(&pool, snapshot, 8);

    if (!contains(snapshot, copied, "A pays X 1 coins") ||
        !contains(snapshot, copied, "C pays X 3 coins") ||
        !contains(snapshot, copied, "D pays X 4 coins")) {
        fprintf(stderr, "A transaction that should have survived was removed.\n");
        mempool_destroy(&pool);
        return 1;
    }

    if (contains(snapshot, copied, "B pays X 2 coins") ||
        contains(snapshot, copied, "E pays X 5 coins")) {
        fprintf(stderr, "A transaction that should have been removed is still present.\n");
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_remove_included_adjacent\n");

    return 0;
}

static int test_mempool_remove_included_last_element(void)
{
    mempool_t pool;
    char *included[1];

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    mempool_add(&pool, "A pays X 1 coins");
    mempool_add(&pool, "B pays X 2 coins");
    mempool_add(&pool, "C pays X 3 coins");

    included[0] = "C pays X 3 coins";

    mempool_remove_included(&pool, included, 1);

    if (mempool_size(&pool) != 2) {
        fprintf(
            stderr,
            "Removing the last element left %zu transactions, expected 2.\n",
            mempool_size(&pool)
        );
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_remove_included_last_element\n");

    return 0;
}

static int test_mempool_remove_included_all(void)
{
    mempool_t pool;
    char *included[3];

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    mempool_add(&pool, "A pays X 1 coins");
    mempool_add(&pool, "B pays X 2 coins");
    mempool_add(&pool, "C pays X 3 coins");

    included[0] = "A pays X 1 coins";
    included[1] = "B pays X 2 coins";
    included[2] = "C pays X 3 coins";

    mempool_remove_included(&pool, included, 3);

    if (mempool_size(&pool) != 0) {
        fprintf(
            stderr,
            "Removing every transaction left %zu behind, expected 0.\n",
            mempool_size(&pool)
        );
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_remove_included_all\n");

    return 0;
}

static int test_mempool_remove_included_no_match(void)
{
    mempool_t pool;
    char *included[1];

    if (mempool_init(&pool) != PROJECT_OK) {
        return 1;
    }

    mempool_add(&pool, "A pays X 1 coins");
    mempool_add(&pool, "B pays X 2 coins");

    included[0] = "Z pays X 9 coins";

    mempool_remove_included(&pool, included, 1);

    if (mempool_size(&pool) != 2) {
        fprintf(
            stderr,
            "A no-op removal changed the pool size to %zu, expected 2.\n",
            mempool_size(&pool)
        );
        mempool_destroy(&pool);
        return 1;
    }

    mempool_destroy(&pool);

    printf("PASS: mempool_remove_included_no_match\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_mempool_init_and_destroy();
    failed_tests += test_mempool_add();
    failed_tests += test_mempool_add_rejects_invalid();
    failed_tests += test_mempool_add_respects_capacity();
    failed_tests += test_mempool_snapshot_respects_max_count();
    failed_tests += test_mempool_remove_included_adjacent();
    failed_tests += test_mempool_remove_included_last_element();
    failed_tests += test_mempool_remove_included_all();
    failed_tests += test_mempool_remove_included_no_match();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d mempool test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All mempool tests passed.\n");

    return 0;
}
