#include "validation.h"
#include "errors.h"

#include <ctype.h>
#include <regex.h>
#include <stddef.h>
#include <string.h>

int validate_transaction(const char *transaction)
{
    static const char pattern[] =
        "^[[:alnum:]]+ pays [[:alnum:]]+ "
        "[1-9][0-9]* coins$";

    regex_t regex;
    int regex_result;
    size_t transaction_length;

    if (transaction == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    transaction_length = strlen(transaction);

    if (transaction_length == 0 ||
        transaction_length >= MAX_TRANSACTION_LENGTH) {
        return ERR_INVALID_TRANSACTION;
    }

    regex_result = regcomp(
        &regex,
        pattern,
        REG_EXTENDED | REG_NOSUB
    );

    if (regex_result != 0) {
        return ERR_INVALID_TRANSACTION;
    }

    regex_result = regexec(
        &regex,
        transaction,
        0,
        NULL,
        0
    );

    regfree(&regex);

    if (regex_result != 0) {
        return ERR_INVALID_TRANSACTION;
    }

    return PROJECT_OK;
}

int validate_hash_string(const char *hash)
{
    if (hash == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (strlen(hash) != SHA256_HEX_LENGTH) {
        return ERR_INVALID_BLOCK;
    }

    for (size_t index = 0;
         index < SHA256_HEX_LENGTH;
         index++) {
        unsigned char character =
            (unsigned char)hash[index];

        /*
         * We accept only lowercase hexadecimal characters because all
         * hashes produced by this project use lowercase output.
         */
        if (!isdigit(character) &&
            !(character >= 'a' && character <= 'f')) {
            return ERR_INVALID_BLOCK;
        }
    }

    return PROJECT_OK;
}
