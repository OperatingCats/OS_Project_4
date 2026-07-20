#include "validation.h"
#include "errors.h"

#include <stdio.h>

static int expect_valid_transaction(const char *transaction)
{
    int result = validate_transaction(transaction);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "Expected valid transaction: \"%s\"\n",
            transaction
        );

        return 1;
    }

    return 0;
}

static int expect_invalid_transaction(const char *transaction)
{
    int result = validate_transaction(transaction);

    if (result != ERR_INVALID_TRANSACTION) {
        fprintf(
            stderr,
            "Expected invalid transaction: \"%s\"\n",
            transaction
        );

        return 1;
    }

    return 0;
}

static int test_valid_transactions(void)
{
    int failures = 0;

    failures += expect_valid_transaction(
        "Alice pays Bob 10 coins"
    );

    failures += expect_valid_transaction(
        "User1 pays User2 500 coins"
    );

    failures += expect_valid_transaction(
        "A pays B 1 coins"
    );

    if (failures != 0) {
        return 1;
    }

    printf("PASS: valid transactions\n");

    return 0;
}

static int test_invalid_transactions(void)
{
    int failures = 0;

    failures += expect_invalid_transaction(
        ""
    );

    failures += expect_invalid_transaction(
        "Alice gives Bob 10 coins"
    );

    failures += expect_invalid_transaction(
        "Alice pays Bob 0 coins"
    );

    failures += expect_invalid_transaction(
        "Alice pays Bob -5 coins"
    );

    failures += expect_invalid_transaction(
        "Alice pays Bob 10"
    );

    failures += expect_invalid_transaction(
        "Alice  pays Bob 10 coins"
    );

    failures += expect_invalid_transaction(
        "Alice pays Bob 01 coins"
    );

    failures += expect_invalid_transaction(
        "Alice pays Bob ten coins"
    );

    if (failures != 0) {
        return 1;
    }

    printf("PASS: invalid transactions rejected\n");

    return 0;
}

static int test_null_transaction(void)
{
    int result = validate_transaction(NULL);

    if (result != ERR_INVALID_ARGUMENT) {
        fprintf(
            stderr,
            "NULL transaction returned %d instead of %d.\n",
            result,
            ERR_INVALID_ARGUMENT
        );

        return 1;
    }

    printf("PASS: NULL transaction rejected\n");

    return 0;
}

static int test_valid_hash(void)
{
    const char *hash =
        "0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef";

    int result = validate_hash_string(hash);

    if (result != PROJECT_OK) {
        fprintf(stderr, "Valid hash was rejected.\n");
        return 1;
    }

    printf("PASS: valid hash\n");

    return 0;
}

static int test_invalid_hashes(void)
{
    const char *too_short = "abcdef";

    const char *uppercase =
        "0123456789ABCDEF0123456789abcdef"
        "0123456789abcdef0123456789abcdef";

    const char *invalid_character =
        "g123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef";

    if (validate_hash_string(too_short) != ERR_INVALID_BLOCK) {
        fprintf(stderr, "Short hash was not rejected.\n");
        return 1;
    }

    if (validate_hash_string(uppercase) != ERR_INVALID_BLOCK) {
        fprintf(stderr, "Uppercase hash was not rejected.\n");
        return 1;
    }

    if (validate_hash_string(invalid_character) !=
        ERR_INVALID_BLOCK) {
        fprintf(
            stderr,
            "Hash with invalid character was not rejected.\n"
        );

        return 1;
    }

    if (validate_hash_string(NULL) != ERR_INVALID_ARGUMENT) {
        fprintf(stderr, "NULL hash was not rejected.\n");
        return 1;
    }

    printf("PASS: invalid hashes rejected\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_valid_transactions();
    failed_tests += test_invalid_transactions();
    failed_tests += test_null_transaction();
    failed_tests += test_valid_hash();
    failed_tests += test_invalid_hashes();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d validation test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All validation tests passed.\n");

    return 0;
}