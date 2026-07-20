#include "crypto.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

static int test_sha256_abc(void)
{
    const char *input = "abc";

    const char *expected =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";

    char actual[SHA256_HEX_STRING_SIZE];

    int result = sha256_hex(
        input,
        strlen(input),
        actual
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "sha256_hex failed: %s\n",
            project_error_string(result)
        );

        return 1;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "Test SHA-256(\"abc\") failed.\n");
        fprintf(stderr, "Expected: %s\n", expected);
        fprintf(stderr, "Actual:   %s\n", actual);

        return 1;
    }

    printf("PASS: SHA-256(\"abc\")\n");

    return 0;
}

static int test_sha256_empty_string(void)
{
    const char *expected =
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855";

    char actual[SHA256_HEX_STRING_SIZE];

    int result = sha256_hex(
        "",
        0,
        actual
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "sha256_hex failed: %s\n",
            project_error_string(result)
        );

        return 1;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "Empty-string SHA-256 test failed.\n");
        fprintf(stderr, "Expected: %s\n", expected);
        fprintf(stderr, "Actual:   %s\n", actual);

        return 1;
    }

    printf("PASS: SHA-256 empty string\n");

    return 0;
}

static int test_invalid_output(void)
{
    int result = sha256_hex(
        "abc",
        3,
        NULL
    );

    if (result != ERR_INVALID_ARGUMENT) {
        fprintf(
            stderr,
            "Invalid-output test failed. Returned: %d\n",
            result
        );

        return 1;
    }

    printf("PASS: invalid output argument\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_sha256_abc();
    failed_tests += test_sha256_empty_string();
    failed_tests += test_invalid_output();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d crypto test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All crypto tests passed.\n");

    return 0;
}