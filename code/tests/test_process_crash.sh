#!/usr/bin/env bash
# Verifies Bootstrap notices a dead child and keeps running instead of
# hanging. Kills a real running Miner with SIGKILL to simulate a crash,
# rather than relying on a missing binary causing an exec() failure --
# that stand-in only worked back when the Miner binary genuinely didn't
# exist yet, and stopped meaning anything once the project actually builds.
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

"$BOOTSTRAP_BIN" 1 1 0 <&3 >"$OUT_LOG" 2>&1 &
BOOTSTRAP_PID=$!

sleep 1

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap exited immediately"
    cat "$OUT_LOG"
    exit 1
fi

echo "status" >&3
sleep 1

MINER_PID="$(awk '$1 == "MINER" {print $3; exit}' "$OUT_LOG")"

if [ -z "$MINER_PID" ]; then
    echo "FAIL: could not read the miner's PID from the status table"
    cat "$OUT_LOG"
    kill -9 "$BOOTSTRAP_PID" 2>/dev/null
    exit 1
fi

echo "killing miner (pid $MINER_PID) to simulate a crash"
kill -9 "$MINER_PID" 2>/dev/null

# Give SIGCHLD time to interrupt bootstrap's fgets() and reap the child.
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
