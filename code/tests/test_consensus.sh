#!/usr/bin/env bash
# Verifies Node 0's consensus path end to end: a correctly-formed block
# proposal that links to the current chain tip is validated, appended,
# and the chain height increases by exactly one.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NODE_BIN="$REPO_ROOT/bin/node"
PROPOSE_BIN="$REPO_ROOT/bin/test_propose_valid_block"
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

if ! echo "$PROPOSE_OUTPUT" | grep -q "reply type=11"; then
    echo "FAIL: valid block proposal was not accepted"
    echo "$PROPOSE_OUTPUT"
    cat "$RUNTIME_DIR/node.out"
    exit 1
fi
echo "PASS: valid block proposal was accepted"

HEIGHT_AFTER="$("$QUERY_BIN" "$SOCK_PATH" height | grep -A1 '^--- reply' | tail -1)"

if [ "$((HEIGHT_AFTER))" -ne "$((HEIGHT_BEFORE + 1))" ]; then
    echo "FAIL: chain height did not increase by exactly one ($HEIGHT_BEFORE -> $HEIGHT_AFTER)"
    exit 1
fi
echo "PASS: chain height increased by exactly one ($HEIGHT_BEFORE -> $HEIGHT_AFTER)"

LOG_FILE="$(ls "$RUNTIME_DIR"/logs/node-*.log 2>/dev/null | head -1)"
    if [ -z "$LOG_FILE" ] || ! grep -q "block committed" "$LOG_FILE"; then
    echo "FAIL: node log did not record the block as accepted and appended"
    [ -n "$LOG_FILE" ] && cat "$LOG_FILE"
    exit 1
fi
echo "PASS: node log confirms block accepted and appended"

echo "All consensus tests passed."
exit 0
