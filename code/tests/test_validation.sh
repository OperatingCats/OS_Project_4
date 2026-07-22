#!/usr/bin/env bash

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERIFY_SCRIPT="$REPO_ROOT/bash/blockchain.sh"
FIXTURES_DIR="$REPO_ROOT/tests/fixtures"

VALID_FILE="$FIXTURES_DIR/valid_blockchain.csv"
INVALID_FILE="$FIXTURES_DIR/invalid_blockchain.csv"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

if [ ! -x "$VERIFY_SCRIPT" ]; then
    fail "$VERIFY_SCRIPT is not executable"
fi

if [ ! -f "$VALID_FILE" ]; then
    fail "$VALID_FILE is missing"
fi

if [ ! -f "$INVALID_FILE" ]; then
    fail "$INVALID_FILE is missing"
fi

OUTPUT="$("$VERIFY_SCRIPT" --verify "$VALID_FILE" 2>&1)"
STATUS=$?

if [ "$STATUS" -ne 0 ]; then
    echo "$OUTPUT" >&2
    fail "valid blockchain was rejected"
fi

echo "PASS: valid blockchain accepted"

OUTPUT="$("$VERIFY_SCRIPT" --verify "$INVALID_FILE" 2>&1)"
STATUS=$?

if [ "$STATUS" -eq 0 ]; then
    echo "$OUTPUT" >&2
    fail "invalid Merkle root was accepted"
fi

if ! grep -qi "merkle" <<< "$OUTPUT"; then
    echo "$OUTPUT" >&2
    fail "invalid Merkle root did not produce a specific Merkle error"
fi

echo "PASS: invalid Merkle root rejected with a specific error"

TMP_DIR="$(mktemp -d)"

cleanup()
{
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

WRONG_INDEX_FILE="$TMP_DIR/wrong_index.csv"

sed \
    's/^0000000000000001,/0000000000000002,/' \
    "$VALID_FILE" > "$WRONG_INDEX_FILE"

OUTPUT="$("$VERIFY_SCRIPT" --verify "$WRONG_INDEX_FILE" 2>&1)"
STATUS=$?

if [ "$STATUS" -eq 0 ]; then
    echo "$OUTPUT" >&2
    fail "wrong block index was accepted"
fi

if ! grep -qi "index" <<< "$OUTPUT"; then
    echo "$OUTPUT" >&2
    fail "wrong index did not produce a specific index error"
fi

echo "PASS: wrong block index rejected with a specific error"

BROKEN_LINK_FILE="$TMP_DIR/broken_link.csv"

sed \
    's/6da77ddcef74a760777c7aa1da1027c20fc297a040a322685574ee432082dd85/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/' \
    "$VALID_FILE" > "$BROKEN_LINK_FILE"

OUTPUT="$("$VERIFY_SCRIPT" --verify "$BROKEN_LINK_FILE" 2>&1)"
STATUS=$?

if [ "$STATUS" -eq 0 ]; then
    echo "$OUTPUT" >&2
    fail "broken previous hash link was accepted"
fi

if ! grep -Eqi "previous hash|chain link|link" <<< "$OUTPUT"; then
    echo "$OUTPUT" >&2
    fail "broken link did not produce a specific link error"
fi

echo "PASS: broken chain link rejected with a specific error"

EMPTY_FILE="$TMP_DIR/empty.csv"
: > "$EMPTY_FILE"

if "$VERIFY_SCRIPT" --verify "$EMPTY_FILE" >/dev/null 2>&1; then
    fail "empty blockchain file was accepted"
fi

echo "PASS: empty blockchain file rejected"

HEADER_ONLY_FILE="$TMP_DIR/header_only.csv"

printf '%s\n' \
    "index,timestamp,prev_hash,merkle_root,nonce,transactions" \
    > "$HEADER_ONLY_FILE"

if "$VERIFY_SCRIPT" --verify "$HEADER_ONLY_FILE" >/dev/null 2>&1; then
    fail "header-only blockchain file was accepted"
fi

echo "PASS: header-only blockchain file rejected"

echo "All validation tests passed."