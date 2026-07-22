#include "crypto.h"
#include <stdio.h>

int main(void) {
    const char *tx = "Genesis block";
    char root[SHA256_HEX_STRING_SIZE];
    int rc = calculate_merkle_root(&tx, 1, root);
    printf("rc=%d root=%s\n", rc, root);
    return 0;
}
