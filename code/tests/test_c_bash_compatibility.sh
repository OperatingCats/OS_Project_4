#!/usr/bin/env bash

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASH_SCRIPT="$REPO_ROOT/bash/blockchain.sh"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

if [ ! -x "$BASH_SCRIPT" ]; then
    fail "$BASH_SCRIPT is not executable"
fi

TMP_DIR="$(mktemp -d)"

cleanup()
{
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

HELPER_SOURCE="$TMP_DIR/compatibility_helper.c"
HELPER_BIN="$TMP_DIR/compatibility_helper"

cat > "$HELPER_SOURCE" <<'EOF'
#include "crypto.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char output[SHA256_HEX_STRING_SIZE];
    int result;

    if (argc < 3) {
        fprintf(
            stderr,
            "Usage: %s <hash|merkle> <input> [more transactions]\n",
            argv[0]
        );

        return 1;
    }

    if (strcmp(argv[1], "hash") == 0) {
        result = sha256_hex(
            argv[2],
            strlen(argv[2]),
            output
        );
    } else if (strcmp(argv[1], "merkle") == 0) {
        result = calculate_merkle_root(
            (const char *const *)&argv[2],
            (size_t)(argc - 2),
            output
        );
    } else {
        fprintf(stderr, "Unknown operation: %s\n", argv[1]);
        return 1;
    }

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "Operation failed: %s\n",
            project_error_string(result)
        );

        return 1;
    }

    printf("%s\n", output);

    return 0;
}
EOF

gcc \
    -I"$REPO_ROOT/include" \
    -I"$REPO_ROOT/third_party/sha256" \
    -std=c11 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Werror \
    "$REPO_ROOT/common/errors.c" \
    "$REPO_ROOT/common/crypto.c" \
    "$REPO_ROOT/common/block.c" \
    "$REPO_ROOT/third_party/sha256/sha256.c" \
    "$HELPER_SOURCE" \
    -o "$HELPER_BIN" ||
    fail "could not compile the temporary C compatibility helper"

echo "PASS: temporary C compatibility helper compiled"

C_HASH="$("$HELPER_BIN" hash "abc")" ||
    fail "C hash calculation failed"

BASH_HASH="$("$BASH_SCRIPT" --hash 616263)" ||
    fail "Bash hash calculation failed"

if [ "$C_HASH" != "$BASH_HASH" ]; then
    echo "C result:    $C_HASH" >&2
    echo "Bash result: $BASH_HASH" >&2
    fail "C and Bash SHA-256 results differ"
fi

echo "PASS: C and Bash SHA-256 results match"

C_GENESIS_MERKLE="$(
    "$HELPER_BIN" merkle "Genesis block"
)" || fail "C genesis Merkle calculation failed"

BASH_GENESIS_MERKLE="$(
    "$BASH_SCRIPT" --merkle "Genesis block"
)" || fail "Bash genesis Merkle calculation failed"

if [ "$C_GENESIS_MERKLE" != "$BASH_GENESIS_MERKLE" ]; then
    echo "C result:    $C_GENESIS_MERKLE" >&2
    echo "Bash result: $BASH_GENESIS_MERKLE" >&2
    fail "C and Bash single-transaction Merkle roots differ"
fi

echo "PASS: C and Bash single-transaction Merkle roots match"

TX1="Alice pays Bob 10 coins"
TX2="Bob pays Carol 5 coins"
TX3="Carol pays Dave 2 coins"

C_THREE_MERKLE="$(
    "$HELPER_BIN" merkle "$TX1" "$TX2" "$TX3"
)" || fail "C three-transaction Merkle calculation failed"

BASH_THREE_MERKLE="$(
    "$BASH_SCRIPT" --merkle "$TX1::$TX2::$TX3"
)" || fail "Bash three-transaction Merkle calculation failed"

if [ "$C_THREE_MERKLE" != "$BASH_THREE_MERKLE" ]; then
    echo "C result:    $C_THREE_MERKLE" >&2
    echo "Bash result: $BASH_THREE_MERKLE" >&2
    fail "C and Bash three-transaction Merkle roots differ"
fi

echo "PASS: C and Bash odd-level Merkle roots match"

echo "All C/Bash compatibility tests passed."