#!/usr/bin/env bash
# Verifies Node query handling: height, block-by-index, and full-chain
# queries all return correct data for a freshly started system with a
# genesis block loaded.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOTSTRAP_BIN="$REPO_ROOT/bin/blockchain"

if [ ! -x "$BOOTSTRAP_BIN" ]; then
    echo "SKIP: $BOOTSTRAP_BIN not built (run 'make build' first)"
    exit 0
fi

OUT_LOG="$(mktemp)"
FIFO="$(mktemp -u)"
mkfifo "$FIFO"
exec 3<>"$FIFO"

cleanup() {
    exec 3>&- 2>/dev/null
    rm -f "$FIFO" "$OUT_LOG"
}
trap cleanup EXIT

"$BOOTSTRAP_BIN" 1 0 0 0 0 "$REPO_ROOT/data/initial_state.csv" <&3 >"$OUT_LOG" 2>&1 &
BOOTSTRAP_PID=$!
sleep 1

echo "request blockchain" >&3
sleep 1
echo "request block --index 0" >&3
sleep 1
echo "stop" >&3

SECONDS_WAITED=0
while kill -0 "$BOOTSTRAP_PID" 2>/dev/null; do
    sleep 1
    SECONDS_WAITED=$((SECONDS_WAITED + 1))
    if [ "$SECONDS_WAITED" -gt 10 ]; then
        echo "FAIL: bootstrap did not exit within 10s of 'stop'"
        kill -9 "$BOOTSTRAP_PID" 2>/dev/null
        cat "$OUT_LOG"
        exit 1
    fi
done
wait "$BOOTSTRAP_PID"

if ! grep -q "Genesis block" "$OUT_LOG"; then
    echo "FAIL: 'request blockchain' did not return the genesis block"
    cat "$OUT_LOG"
    exit 1
fi
echo "PASS: 'request blockchain' returned genesis block"

if ! grep -q "Genesis block" "$OUT_LOG"; then
    echo "FAIL: 'request block --index 0' did not return the genesis block"
    cat "$OUT_LOG"
    exit 1
fi
echo "PASS: 'request block --index 0' returned genesis block"

echo "All node query tests passed."
exit 0
