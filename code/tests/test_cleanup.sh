#!/usr/bin/env bash
# Verifies that stopping Bootstrap removes every runtime resource: the
# runtime directory and everything inside it (sockets, logs, copied
# initial state), plus that nothing keeps running against it afterward.
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

RUNTIME_DIR="$(grep -o '/tmp/blockchain_[0-9]*' "$OUT_LOG" | head -n1)"

if [ -z "$RUNTIME_DIR" ]; then
    echo "FAIL: could not determine runtime directory"
    kill -9 "$BOOTSTRAP_PID" 2>/dev/null
    exit 1
fi

echo "stop" >&3
wait "$BOOTSTRAP_PID"

if [ -e "$RUNTIME_DIR" ]; then
    echo "FAIL: runtime directory still exists after shutdown: $RUNTIME_DIR"
    exit 1
fi

echo "PASS: runtime directory fully removed"

if pgrep -f "$RUNTIME_DIR" >/dev/null 2>&1; then
    echo "FAIL: a process referencing the old runtime directory is still running"
    exit 1
fi

echo "PASS: no leftover processes reference the removed runtime directory"
echo "All cleanup tests passed."
