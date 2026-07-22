#!/usr/bin/env bash
# Verifies the spec's named edge case: two proposals racing for the same
# block index. Exactly one must be accepted, the other rejected, and the
# chain height must advance by exactly one, not two.
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

OUT_A="$(mktemp)"
OUT_B="$(mktemp)"

# Fire both proposals essentially simultaneously.
"$PROPOSE_BIN" "$SOCK_PATH" >"$OUT_A" 2>&1 &
PID_A=$!
"$PROPOSE_BIN" "$SOCK_PATH" >"$OUT_B" 2>&1 &
PID_B=$!

wait "$PID_A"
wait "$PID_B"

ACCEPTED=0
REJECTED=0

if grep -q "reply type=11" "$OUT_A"; then ACCEPTED=$((ACCEPTED + 1)); fi
if grep -q "reply type=5" "$OUT_A"; then REJECTED=$((REJECTED + 1)); fi
if grep -q "reply type=11" "$OUT_B"; then ACCEPTED=$((ACCEPTED + 1)); fi
if grep -q "reply type=5" "$OUT_B"; then REJECTED=$((REJECTED + 1)); fi

rm -f "$OUT_A" "$OUT_B"

if [ "$ACCEPTED" -ne 1 ] || [ "$REJECTED" -ne 1 ]; then
    echo "FAIL: expected exactly 1 accepted and 1 rejected, got accepted=$ACCEPTED rejected=$REJECTED"
    exit 1
fi
echo "PASS: exactly one proposal accepted, one rejected"

HEIGHT="$("$QUERY_BIN" "$SOCK_PATH" height | grep -A1 '^--- reply' | tail -1)"
if [ "$((HEIGHT))" -ne 2 ]; then
    echo "FAIL: chain height should be exactly 2 after one accepted proposal, got $HEIGHT"
    exit 1
fi
echo "PASS: chain height advanced by exactly one block ($HEIGHT)"

echo "All simultaneous proposal tests passed."
exit 0
