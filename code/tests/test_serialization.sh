#!/usr/bin/env bash

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_BIN="$REPO_ROOT/bin/test_block"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

if [ ! -x "$TEST_BIN" ]; then
    fail "$TEST_BIN is not built; run 'make build' first"
fi

OUTPUT="$("$TEST_BIN" 2>&1)"
STATUS=$?

if [ "$STATUS" -ne 0 ]; then
    echo "$OUTPUT" >&2
    fail "block test executable returned status $STATUS"
fi

if ! grep -q "^PASS: block serialization round trip$" <<< "$OUTPUT"; then
    echo "$OUTPUT" >&2
    fail "serialization round-trip test did not run or did not pass"
fi

echo "PASS: block serialization round trip"

if ! grep -q "^PASS: block CSV round trip$" <<< "$OUTPUT"; then
    echo "$OUTPUT" >&2
    fail "CSV round-trip test did not run or did not pass"
fi

echo "PASS: block CSV round trip"

if ! grep -q "^PASS: invalid CSV rejected$" <<< "$OUTPUT"; then
    echo "$OUTPUT" >&2
    fail "malformed CSV rejection test did not run or did not pass"
fi

echo "PASS: malformed serialized block is rejected"

echo "All serialization tests passed."