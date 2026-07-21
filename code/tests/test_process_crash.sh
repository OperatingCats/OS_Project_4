#!/usr/bin/env bash
# Verifies Bootstrap survives a child process dying unexpectedly: it
# should notice the child is gone and keep running instead of hanging.
# A missing/unbuilt child binary causes exec() itself to fail, which is
# used here as a cheap stand-in for a real crash -- Bootstrap sees the
# same "child exited" signal either way.
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

# Give the child time to spawn and exit.
sleep 2

echo "status" >&3
sleep 1

if ! grep -q "terminated" "$OUT_LOG"; then
    echo "FAIL: bootstrap never reported the crashed child as terminated"
    cat "$OUT_LOG"
    kill -9 "$BOOTSTRAP_PID" 2>/dev/null
    exit 1
fi

echo "PASS: crashed child detected and reaped"

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap itself exited/hung after the child crash"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap remained responsive after the crash"

echo "stop" >&3

if ! wait "$BOOTSTRAP_PID"; then
    echo "FAIL: bootstrap did not shut down cleanly after the crash"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap shut down cleanly after the crash"
echo "All process-crash tests passed."
