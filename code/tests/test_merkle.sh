#!/usr/bin/env bash

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT="$REPO_ROOT/bash/blockchain.sh"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

if [ ! -x "$SCRIPT" ]; then
    fail "$SCRIPT is not executable"
fi

check_merkle()
{
    local transactions="$1"
    local expected="$2"
    local description="$3"
    local actual

    actual="$("$SCRIPT" --merkle "$transactions")" ||
        fail "--merkle rejected valid input: $description"

    if [ "$actual" != "$expected" ]; then
        echo "Expected: $expected" >&2
        echo "Actual:   $actual" >&2
        fail "wrong Merkle root: $description"
    fi

    echo "PASS: $description"
}

check_merkle \
    "Genesis block" \
    "b815a93dd7f59058a27e63558ba5aa6445d851f316070ec13db673d5ab38e0cc" \
    "single transaction with empty-hash padding"

check_merkle \
    "Alice pays Bob 10 coins::Bob pays Carol 5 coins" \
    "d05a48e5dee6b30fb2a14dd6fd33b6ff56d8f641cbbb249b8f95959baa1c4df8" \
    "two transactions"

check_merkle \
    "Alice pays Bob 10 coins::Bob pays Carol 5 coins::Carol pays Dave 2 coins" \
    "29859cc1a5d5035df927c394bc2befa216b2d2655a2ea361803ef651b469e8fe" \
    "three transactions with padding"

if "$SCRIPT" --merkle "" >/dev/null 2>&1; then
    fail "--merkle accepted empty input"
fi

echo "PASS: --merkle rejects empty input"

if "$SCRIPT" --merkle "::" >/dev/null 2>&1; then
    fail "--merkle accepted an empty transaction list"
fi

echo "PASS: --merkle rejects empty transactions"

if "$SCRIPT" --merkle "Alice pays Bob 10 coins::" >/dev/null 2>&1; then
    fail "--merkle accepted a trailing separator"
fi

echo "PASS: --merkle rejects trailing separators"

if "$SCRIPT" --merkle "::Alice pays Bob 10 coins" >/dev/null 2>&1; then
    fail "--merkle accepted a leading separator"
fi

echo "PASS: --merkle rejects leading separators"

echo "All Merkle tests passed."