#!/usr/bin/env bash
# Tests basic Bootstrap process lifecycle: startup, runtime directory
# creation, the interactive command loop, and clean shutdown.
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

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap exited immediately"
    cat "$OUT_LOG"
    exit 1
fi

RUNTIME_DIR="$(grep -o '/tmp/blockchain_[0-9]*' "$OUT_LOG" | head -n1)"

if [ -z "$RUNTIME_DIR" ] || [ ! -d "$RUNTIME_DIR" ]; then
    echo "FAIL: runtime directory was not created"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: runtime directory created ($RUNTIME_DIR)"

echo "status" >&3
sleep 1

if ! grep -q "TYPE" "$OUT_LOG"; then
    echo "FAIL: status command produced no process table"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: status command responded"

echo "stop" >&3

if ! wait "$BOOTSTRAP_PID"; then
    echo "FAIL: bootstrap did not exit cleanly after 'stop'"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap exited cleanly after 'stop'"

if [ -d "$RUNTIME_DIR" ]; then
    echo "FAIL: runtime directory was not removed on shutdown"
    exit 1
fi

echo "PASS: runtime directory removed on shutdown"
echo "All bootstrap tests passed."
