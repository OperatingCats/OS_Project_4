#!/usr/bin/env bash
# Verifies Bootstrap tracks multiple Miners independently: each crashed
# Miner is reaped and reported under its own id, without Bootstrap
# hanging or losing track of the survivors.
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

"$BOOTSTRAP_BIN" 1 2 0 <&3 >"$OUT_LOG" 2>&1 &
BOOTSTRAP_PID=$!

sleep 2
echo "status" >&3
sleep 1

MINER_LINES="$(grep -c '^MINER' "$OUT_LOG" || true)"

if [ "$MINER_LINES" -lt 2 ]; then
    echo "FAIL: expected 2 tracked miners in the process table, found $MINER_LINES"
    cat "$OUT_LOG"
    kill -9 "$BOOTSTRAP_PID" 2>/dev/null
    exit 1
fi

echo "PASS: both miners tracked independently"

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap did not survive tracking two crashed miners"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap stayed up with two miners down"

echo "stop" >&3

if ! wait "$BOOTSTRAP_PID"; then
    echo "FAIL: bootstrap did not shut down cleanly"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap shut down cleanly"
echo "All miner-restart tests passed."
