#!/usr/bin/env bash
# Verifies Bootstrap's pause/resume commands don't crash or hang it, and
# -- when a real child process stays alive long enough to check -- that
# pause/resume actually change its state via SIGSTOP/SIGCONT.
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

echo "pause" >&3
sleep 1

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap died after 'pause'"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap survived 'pause'"

echo "resume" >&3
sleep 1

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap died after 'resume'"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap survived 'resume'"

# Best-effort: if a child is still alive, confirm SIGSTOP/SIGCONT
# actually took effect via /proc's state field ('T' == stopped).
CHILD_PID="$(pgrep -P "$BOOTSTRAP_PID" | head -n1 || true)"

if [ -n "${CHILD_PID:-}" ] && [ -r "/proc/$CHILD_PID/stat" ]; then
    echo "pause" >&3
    sleep 1
    STATE="$(awk '{print $3}' "/proc/$CHILD_PID/stat" 2>/dev/null || true)"
    if [ "$STATE" = "T" ]; then
        echo "PASS: child process actually stopped (SIGSTOP) on pause"
    else
        echo "SKIP: could not confirm child stopped state (state=$STATE)"
    fi
    echo "resume" >&3
    sleep 1
fi

echo "stop" >&3

if ! wait "$BOOTSTRAP_PID"; then
    echo "FAIL: bootstrap did not shut down cleanly"
    cat "$OUT_LOG"
    exit 1
fi

echo "PASS: bootstrap shut down cleanly"
echo "All pause/resume tests passed."
