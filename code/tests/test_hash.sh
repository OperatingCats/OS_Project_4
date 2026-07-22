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

EXPECTED="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"

ACTUAL="$("$SCRIPT" --hash 616263)" ||
    fail "--hash rejected valid input"

if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "Expected: $EXPECTED" >&2
    echo "Actual:   $ACTUAL" >&2
    fail "wrong SHA-256 result for hexadecimal input 616263"
fi

echo "PASS: --hash computes SHA-256 of decoded hexadecimal bytes"

if "$SCRIPT" --hash "" >/dev/null 2>&1; then
    fail "--hash accepted empty input"
fi

echo "PASS: --hash rejects empty input"

if "$SCRIPT" --hash abc >/dev/null 2>&1; then
    fail "--hash accepted odd-length hexadecimal input"
fi

echo "PASS: --hash rejects odd-length hexadecimal input"

if "$SCRIPT" --hash 12xz >/dev/null 2>&1; then
    fail "--hash accepted non-hexadecimal input"
fi

echo "PASS: --hash rejects non-hexadecimal input"

echo "All hash tests passed."