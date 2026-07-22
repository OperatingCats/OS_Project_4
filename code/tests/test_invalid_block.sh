#!/usr/bin/env bash
# Verifies that Node 0 correctly rejects a structurally invalid block
# proposal (bad merkle root) instead of appending it, and that the chain
# height remains unchanged afterward.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NODE_BIN="$REPO_ROOT/bin/node"
PROPOSE_BIN="$REPO_ROOT/bin/test_propose_block"
QUERY_BIN="$REPO_ROOT/bin/test_query"

if [ ! -x "$NODE_BIN" ] || [ ! -x "$PROPOSE_BIN" ] || [ ! -x "$QUERY_BIN" ]; then
    echo "SKIP: required binaries not built (run 'make build' and compile test clients first)"
    exit 0
fi

RUNTIME_DIR="$(mktemp -d)"
cp "$REPO_ROOT/data/initial_state.csv" "$RUNTIME_DIR/"

cleanup() {
    kill "$NODE_PID" 2>/dev/null
    wait "$NODE_PID" 2>/dev/null
    rm -rf "$RUNTIME_DIR"
}
trap cleanup EXIT

"$NODE_BIN" 0 "$RUNTIME_DIR" >"$RUNTIME_DIR/node.out" 2>&1 &
NODE_PID=$!
sleep 1

SOCK_PATH="$RUNTIME_DIR/node_0.sock"

HEIGHT_BEFORE="$("$QUERY_BIN" "$SOCK_PATH" height | grep -A1 '^--- reply' | tail -1)"

PROPOSE_OUTPUT="$("$PROPOSE_BIN" "$SOCK_PATH")"

if ! echo "$PROPOSE_OUTPUT" | grep -q "invalid proposal"; then
    echo "FAIL: invalid block was not rejected"
    echo "$PROPOSE_OUTPUT"
    exit 1
fi
echo "PASS: invalid block was rejected"

HEIGHT_AFTER="$("$QUERY_BIN" "$SOCK_PATH" height | grep -A1 '^--- reply' | tail -1)"

if [ "$HEIGHT_BEFORE" != "$HEIGHT_AFTER" ]; then
    echo "FAIL: chain height changed after invalid proposal ($HEIGHT_BEFORE -> $HEIGHT_AFTER)"
    exit 1
fi
echo "PASS: chain height unchanged after invalid proposal ($HEIGHT_AFTER)"

echo "All invalid block tests passed."
exit 0
