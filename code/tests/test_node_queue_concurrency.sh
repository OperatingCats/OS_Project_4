#!/usr/bin/env bash
# Wires common/node_queue.c into the automated test suite. Builds the
# stress test if needed, then runs it: many producer threads push more
# items than the queue's fixed capacity while many consumer threads pop
# concurrently, verifying nothing is lost or duplicated.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$REPO_ROOT/bin/test_node_queue_manual"

if [ ! -x "$BIN" ]; then
    gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$REPO_ROOT/include" -pthread \
        "$REPO_ROOT/common/node_queue.c" \
        "$REPO_ROOT/tests/test_node_queue_manual.c" \
        -o "$BIN"
fi

if [ ! -x "$BIN" ]; then
    echo "FAIL: could not build test_node_queue_manual"
    exit 1
fi

if ! "$BIN"; then
    echo "FAIL: node queue concurrency test failed"
    exit 1
fi

exit 0
