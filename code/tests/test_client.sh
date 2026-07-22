#!/usr/bin/env bash

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

NODE_BIN="$REPO_ROOT/bin/node"
MINER_BIN="$REPO_ROOT/bin/miner"
CLIENT_BIN="$REPO_ROOT/bin/client"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

for binary in "$NODE_BIN" "$MINER_BIN" "$CLIENT_BIN"; do
    if [ ! -x "$binary" ]; then
        fail "$binary is not built; run 'make build' first"
    fi
done

RUNTIME_DIR="$(mktemp -d)"
mkdir -p "$RUNTIME_DIR/logs"

cp \
    "$REPO_ROOT/data/initial_state.csv" \
    "$RUNTIME_DIR/initial_state.csv" ||
    fail "could not copy the initial blockchain state"

NODE_PID=""
MINER_PID=""
CLIENT_PID=""

cleanup()
{
    if [ -n "$CLIENT_PID" ]; then
        kill -TERM "$CLIENT_PID" 2>/dev/null || true
        wait "$CLIENT_PID" 2>/dev/null || true
    fi

    if [ -n "$MINER_PID" ]; then
        kill -TERM "$MINER_PID" 2>/dev/null || true
        wait "$MINER_PID" 2>/dev/null || true
    fi

    if [ -n "$NODE_PID" ]; then
        kill -TERM "$NODE_PID" 2>/dev/null || true
        wait "$NODE_PID" 2>/dev/null || true
    fi

    rm -rf "$RUNTIME_DIR"
}

trap cleanup EXIT

"$NODE_BIN" 0 "$RUNTIME_DIR" \
    >"$RUNTIME_DIR/node.out" 2>&1 &
NODE_PID=$!

for _ in 1 2 3 4 5; do
    if [ -S "$RUNTIME_DIR/node_0.sock" ]; then
        break
    fi

    sleep 1
done

if [ ! -S "$RUNTIME_DIR/node_0.sock" ]; then
    cat "$RUNTIME_DIR/node.out" >&2
    fail "Node 0 socket was not created"
fi

echo "PASS: Node 0 started"

"$MINER_BIN" 0 "$RUNTIME_DIR" 1 \
    >"$RUNTIME_DIR/miner.out" 2>&1 &
MINER_PID=$!

for _ in 1 2 3 4 5; do
    if [ -S "$RUNTIME_DIR/miner_0.sock" ]; then
        break
    fi

    sleep 1
done

if [ ! -S "$RUNTIME_DIR/miner_0.sock" ]; then
    cat "$RUNTIME_DIR/miner.out" >&2
    fail "Miner socket was not created"
fi

echo "PASS: Miner started"

"$CLIENT_BIN" 0 "$RUNTIME_DIR" 1 \
    >"$RUNTIME_DIR/client.out" 2>&1 &
CLIENT_PID=$!

CLIENT_LOG=""

for _ in 1 2 3 4 5; do
    CLIENT_LOG="$(
        find "$RUNTIME_DIR/logs" \
            -maxdepth 1 \
            -type f \
            -name 'client-*.log' \
            | head -n 1
    )"

    if [ -n "$CLIENT_LOG" ]; then
        break
    fi

    sleep 1
done

if [ -z "$CLIENT_LOG" ] || [ ! -f "$CLIENT_LOG" ]; then
    cat "$RUNTIME_DIR/client.out" >&2
    fail "Client log file was not created"
fi

echo "PASS: Client started and created its log"

ACCEPTED=0

for _ in 1 2 3 4 5 6 7 8 9 10; do
    if grep -q "generated transaction:" "$CLIENT_LOG" &&
       grep -q "transaction accepted by miner 0:" "$CLIENT_LOG"; then
        ACCEPTED=1
        break
    fi

    sleep 1
done

if [ "$ACCEPTED" -ne 1 ]; then
    echo "--- client log ---" >&2
    cat "$CLIENT_LOG" >&2
    echo "--- miner output ---" >&2
    cat "$RUNTIME_DIR/miner.out" >&2
    fail "Client did not generate and submit an accepted transaction"
fi

echo "PASS: Client generated a valid transaction"
echo "PASS: Miner accepted the Client transaction"

if ! grep -Eq \
    '^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}\] client 0:' \
    "$CLIENT_LOG"; then
    cat "$CLIENT_LOG" >&2
    fail "Client log does not use the required millisecond timestamp format"
fi

echo "PASS: Client log uses millisecond timestamps"

kill -TERM "$CLIENT_PID"

if ! wait "$CLIENT_PID"; then
    fail "Client did not terminate cleanly after SIGTERM"
fi

CLIENT_PID=""

if ! grep -q "shutting down" "$CLIENT_LOG"; then
    cat "$CLIENT_LOG" >&2
    fail "Client did not log its clean shutdown"
fi

echo "PASS: Client terminated cleanly after SIGTERM"
echo "All Client tests passed."