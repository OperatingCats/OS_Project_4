#!/usr/bin/env bash
# Verifies that a Node which missed intermediate blocks correctly detects
# the gap, requests the missing blocks from the coordinator, validates
# them, and catches up to the correct chain height.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NODE_BIN="$REPO_ROOT/bin/node"
PROPOSE_BIN="$REPO_ROOT/bin/test_propose_next_block"
RELAY_BIN="$REPO_ROOT/bin/test_relay_commit"
QUERY_BIN="$REPO_ROOT/bin/test_query"

if [ ! -x "$NODE_BIN" ] || [ ! -x "$PROPOSE_BIN" ] || [ ! -x "$RELAY_BIN" ] || [ ! -x "$QUERY_BIN" ]; then
    echo "SKIP: required binaries not built (run 'make build' and compile test clients first)"
    exit 0
fi

RUNTIME_DIR="$(mktemp -d)"
cp "$REPO_ROOT/data/initial_state.csv" "$RUNTIME_DIR/"

cleanup() {
    kill "$NODE0_PID" "$NODE1_PID" 2>/dev/null
    wait "$NODE0_PID" "$NODE1_PID" 2>/dev/null
    rm -rf "$RUNTIME_DIR"
}
trap cleanup EXIT

"$NODE_BIN" 0 "$RUNTIME_DIR" >"$RUNTIME_DIR/node0.out" 2>&1 &
NODE0_PID=$!
sleep 1

SOCK0="$RUNTIME_DIR/node_0.sock"

# Grow node 0's chain to height 3 before node 1 ever starts, so node 1
# misses these blocks rather than receiving them live.
"$PROPOSE_BIN" "$SOCK0" "Alice pays Bob 5 coins" >/dev/null
"$PROPOSE_BIN" "$SOCK0" "Bob pays Carol 3 coins" >/dev/null

HEIGHT0="$("$QUERY_BIN" "$SOCK0" height | grep -A1 '^--- reply' | tail -1)"
if [ "$((HEIGHT0))" -ne 3 ]; then
    echo "FAIL: could not bring node 0 to height 3 before starting node 1 (got $HEIGHT0)"
    exit 1
fi

# start node 1, which only has genesis.
"$NODE_BIN" 1 "$RUNTIME_DIR" >"$RUNTIME_DIR/node1.out" 2>&1 &
NODE1_PID=$!
sleep 1

SOCK1="$RUNTIME_DIR/node_1.sock"

HEIGHT1_BEFORE="$("$QUERY_BIN" "$SOCK1" height | grep -A1 '^--- reply' | tail -1)"
if [ "$((HEIGHT1_BEFORE))" -ne 1 ]; then
    echo "FAIL: node 1 should start at height 1 (genesis only), got $HEIGHT1_BEFORE"
    exit 1
fi
echo "PASS: node 1 starts behind, at height 1"

# Simulate a commit broadcast for the latest block reaching node 1,
# which is far ahead of what node 1 has.
"$RELAY_BIN" "$SOCK0" "$SOCK1" 2 >/dev/null
sleep 1

HEIGHT1_AFTER="$("$QUERY_BIN" "$SOCK1" height | grep -A1 '^--- reply' | tail -1)"
if [ "$((HEIGHT1_AFTER))" -ne 3 ]; then
    echo "FAIL: node 1 did not recover to height 3 after receiving a commit ahead of its chain (got $HEIGHT1_AFTER)"
    cat "$RUNTIME_DIR/node1.out"
    exit 1
fi
echo "PASS: node 1 recovered to height 3 after syncing missing blocks"

LOG1="$(ls "$RUNTIME_DIR"/logs/node-*.log 2>/dev/null | xargs grep -l "recovery completed" 2>/dev/null | head -1)"
if [ -z "$LOG1" ]; then
    echo "FAIL: no node log recorded 'recovery completed'"
    exit 1
fi
echo "PASS: node log confirms recovery completed"

echo "All node recovery tests passed."
exit 0
