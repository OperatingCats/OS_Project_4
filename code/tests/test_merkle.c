#include "crypto.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

static int check_merkle_root(
    const char *test_name,
    const char *const transactions[],
    size_t transaction_count,
    const char *expected
)
{
    char actual[SHA256_HEX_STRING_SIZE];

    int result = calculate_merkle_root(
        transactions,
        transaction_count,
        actual
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "%s failed with error: %s\n",
            test_name,
            project_error_string(result)
        );

        return 1;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s failed.\n", test_name);
        fprintf(stderr, "Expected: %s\n", expected);
        fprintf(stderr, "Actual:   %s\n", actual);

        return 1;
    }

    printf("PASS: %s\n", test_name);

    return 0;
}

static int test_single_transaction(void)
{
    const char *transactions[] = {
        "Alice pays Bob 10 coins"
    };

    const char *expected =
        "489cdba1288d6741c7b929eacbc97b43"
        "a60039d6a21fc08a9d641716ba851778";

    return check_merkle_root(
        "Merkle root with one transaction",
        transactions,
        1,
        expected
    );
}

static int test_two_transactions(void)
{
    const char *transactions[] = {
        "Alice pays Bob 10 coins",
        "Bob pays Carol 5 coins"
    };

    const char *expected =
        "d05a48e5dee6b30fb2a14dd6fd33b6ff"
        "56d8f641cbbb249b8f95959baa1c4df8";

    return check_merkle_root(
        "Merkle root with two transactions",
        transactions,
        2,
        expected
    );
}

static int test_three_transactions(void)
{
    const char *transactions[] = {
        "Alice pays Bob 10 coins",
        "Bob pays Carol 5 coins",
        "Carol pays Dave 2 coins"
    };

    const char *expected =
        "29859cc1a5d5035df927c394bc2befa2"
        "16b2d2655a2ea361803ef651b469e8fe";

    return check_merkle_root(
        "Merkle root with three transactions",
        transactions,
        3,
        expected
    );
}

static int test_zero_transactions(void)
{
    char output[SHA256_HEX_STRING_SIZE];

    int result = calculate_merkle_root(
        NULL,
        0,
        output
    );

    if (result != ERR_INVALID_ARGUMENT) {
        fprintf(
            stderr,
            "Zero-transaction test returned %d instead of %d.\n",
            result,
            ERR_INVALID_ARGUMENT
        );

        return 1;
    }

    printf("PASS: zero transactions rejected\n");

    return 0;
}

static int test_null_transaction(void)
{
    const char *transactions[] = {
        "Alice pays Bob 10 coins",
        NULL
    };

    char output[SHA256_HEX_STRING_SIZE];

    int result = calculate_merkle_root(
        transactions,
        2,
        output
    );

    if (result != ERR_INVALID_TRANSACTION) {
        fprintf(
            stderr,
            "NULL transaction test returned %d instead of %d.\n",
            result,
            ERR_INVALID_TRANSACTION
        );

        return 1;
    }

    printf("PASS: NULL transaction rejected\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_single_transaction();
    failed_tests += test_two_transactions();
    failed_tests += test_three_transactions();
    failed_tests += test_zero_transactions();
    failed_tests += test_null_transaction();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d Merkle test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All Merkle tests passed.\n");

    return 0;
}