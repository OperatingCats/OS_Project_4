#include "crypto.h"
#include "errors.h"
#include "sha256.h"

#include <stddef.h>

static void digest_to_hex(
    const unsigned char digest[SHA256_DIGEST_SIZE],
    char output[SHA256_HEX_STRING_SIZE]
)
{
    static const char hexadecimal_digits[] = "0123456789abcdef";

    for (size_t index = 0; index < SHA256_DIGEST_SIZE; index++) {
        output[index * 2] =
            hexadecimal_digits[(digest[index] >> 4) & 0x0f];

        output[index * 2 + 1] =
            hexadecimal_digits[digest[index] & 0x0f];
    }

    output[SHA256_HEX_LENGTH] = '\0';
}

int sha256_bytes(
    const void *data,
    size_t data_size,
    unsigned char output[SHA256_DIGEST_SIZE]
)
{
    SHA256_CTX context;

    if (output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (data == NULL && data_size != 0) {
        return ERR_INVALID_ARGUMENT;
    }

    sha256_init(&context);

    /*
     * For an empty input, sha256_update does not need to be called.
     * This also avoids passing NULL to the third-party function.
     */
    if (data_size > 0) {
        sha256_update(
            &context,
            (const BYTE *)data,
            data_size
        );
    }

    sha256_final(&context, output);

    return PROJECT_OK;
}

int sha256_hex(
    const void *data,
    size_t data_size,
    char output[SHA256_HEX_STRING_SIZE]
)
{
    unsigned char digest[SHA256_DIGEST_SIZE];
    int result;

    if (output == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    result = sha256_bytes(data, data_size, digest);

    if (result != PROJECT_OK) {
        return result;
    }

    digest_to_hex(digest, output);

    return PROJECT_OK;
}