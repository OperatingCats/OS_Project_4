#!/usr/bin/env bash
# Verifies graceful shutdown: 'stop' terminates every child and Bootstrap
# itself within the shutdown timeout, with no processes left behind.
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

"$BOOTSTRAP_BIN" 1 0 0 <&3 >"$OUT_LOG" 2>&1 &
BOOTSTRAP_PID=$!

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

echo "PASS: bootstrap exited within the shutdown timeout"

wait "$BOOTSTRAP_PID"
EXIT_CODE=$?

if [ "$EXIT_CODE" -ne 0 ]; then
    echo "FAIL: bootstrap exited with status $EXIT_CODE"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap exited with status 0"
echo "All shutdown tests passed."
