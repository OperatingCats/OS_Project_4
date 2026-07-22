#!/usr/bin/env bash

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERIFY_SCRIPT="$REPO_ROOT/bash/blockchain.sh"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

if [ ! -x "$VERIFY_SCRIPT" ]; then
    fail "$VERIFY_SCRIPT is not executable"
fi

TMP_DIR="$(mktemp -d)"

cleanup()
{
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

HEADER="index,timestamp,prev_hash,merkle_root,nonce,transactions"

GENESIS_LINE='0000000000000000,0000000067890001,0000000000000000000000000000000000000000000000000000000000000000,b815a93dd7f59058a27e63558ba5aa6445d851f316070ec13db673d5ab38e0cc,0000000000000000,"Genesis block"'

MISSING_FILE="$TMP_DIR/missing.csv"

if "$VERIFY_SCRIPT" --verify "$MISSING_FILE" >/dev/null 2>&1; then
    fail "missing CSV file was accepted"
fi

echo "PASS: missing CSV file rejected"

EMPTY_FILE="$TMP_DIR/empty.csv"
: > "$EMPTY_FILE"

if "$VERIFY_SCRIPT" --verify "$EMPTY_FILE" >/dev/null 2>&1; then
    fail "empty CSV file was accepted"
fi

echo "PASS: empty CSV file rejected"

HEADER_ONLY_FILE="$TMP_DIR/header_only.csv"
printf '%s\n' "$HEADER" > "$HEADER_ONLY_FILE"

if "$VERIFY_SCRIPT" --verify "$HEADER_ONLY_FILE" >/dev/null 2>&1; then
    fail "header-only CSV file was accepted"
fi

echo "PASS: header-only CSV file rejected"

BAD_HEADER_FILE="$TMP_DIR/bad_header.csv"
{
    printf '%s\n' "wrong,header"
    printf '%s\n' "$GENESIS_LINE"
} > "$BAD_HEADER_FILE"

if "$VERIFY_SCRIPT" --verify "$BAD_HEADER_FILE" >/dev/null 2>&1; then
    fail "CSV with invalid header was accepted"
fi

echo "PASS: invalid CSV header rejected"

MALFORMED_FILE="$TMP_DIR/malformed.csv"
{
    printf '%s\n' "$HEADER"
    printf '%s\n' "0000000000000000,missing,fields"
} > "$MALFORMED_FILE"

if "$VERIFY_SCRIPT" --verify "$MALFORMED_FILE" >/dev/null 2>&1; then
    fail "malformed CSV line was accepted"
fi

echo "PASS: malformed CSV line rejected"

VALID_FILE="$TMP_DIR/valid.csv"
{
    printf '%s\n' "$HEADER"
    printf '%s\n' "$GENESIS_LINE"
} > "$VALID_FILE"

OUTPUT="$("$VERIFY_SCRIPT" --verify "$VALID_FILE" 2>&1)"
STATUS=$?

if [ "$STATUS" -ne 0 ]; then
    echo "$OUTPUT" >&2
    fail "valid genesis CSV was rejected"
fi

echo "PASS: valid genesis CSV accepted"

echo "All CSV tests passed."