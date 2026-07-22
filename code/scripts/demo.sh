#!/usr/bin/env bash
# Full project demo scenario: rehearsal tool and live fallback.
#
# Walks the whole system through startup, mining, consensus queries,
# pause/resume, a simulated miner crash, persistence, and shutdown, in the
# same six phases described in scripts/demo_cheatsheet.md. Not required by
# the spec (only build/run/clean are); this exists so the team can rehearse
# the live demo and has something reliable to fall back to if a live typed
# run goes sideways during the presentation.
set -u

NODES=3
MINERS=2
CLIENTS=1
TX_FREQUENCY=5
DIFFICULTY=3
MINE_TIMEOUT_SECONDS=120

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOTSTRAP_BIN="$REPO_ROOT/bin/blockchain"
VERIFY_OUTPUT_CSV="$REPO_ROOT/demo_output.csv"

phase() {
    printf '\n=== %s ===\n' "$1"
}

if [ ! -x "$BOOTSTRAP_BIN" ]; then
    echo "bin/blockchain not built yet, running 'make build'..."
    if ! (cd "$REPO_ROOT" && make build); then
        echo "FAIL: make build did not succeed"
        exit 1
    fi
fi

OUT_LOG="$(mktemp)"
FIFO="$(mktemp -u)"
mkfifo "$FIFO"
exec 3<>"$FIFO"

cleanup() {
    exec 3>&- 2>/dev/null

    if [ -n "${BOOTSTRAP_PID:-}" ] && kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
        echo "cleanup: bootstrap (pid $BOOTSTRAP_PID) still running, terminating it"
        kill -TERM "$BOOTSTRAP_PID" 2>/dev/null
        sleep 2
        kill -0 "$BOOTSTRAP_PID" 2>/dev/null && kill -KILL "$BOOTSTRAP_PID" 2>/dev/null
        wait "$BOOTSTRAP_PID" 2>/dev/null
    fi

    rm -f "$FIFO" "$OUT_LOG" "$VERIFY_OUTPUT_CSV"
}
trap cleanup EXIT

# Sends one command to the bootstrap's interactive prompt and prints
# whatever it wrote to stdout in response, ignoring everything logged
# before this command was sent.
send_cmd() {
    local command="$1"
    local settle_seconds="${2:-1}"
    local offset
    offset="$(wc -c < "$OUT_LOG")"

    echo "$command" >&3
    sleep "$settle_seconds"

    tail -c "+$((offset + 1))" "$OUT_LOG"
}

phase "Phase 1: startup"
echo "> $NODES nodes, $MINERS miners, $CLIENTS client(s), tx every ${TX_FREQUENCY}s, difficulty $DIFFICULTY"

(cd "$REPO_ROOT" && "$BOOTSTRAP_BIN" "$NODES" "$MINERS" "$CLIENTS" "$TX_FREQUENCY" "$DIFFICULTY" data/initial_state.csv) \
    <&3 >"$OUT_LOG" 2>&1 &
BOOTSTRAP_PID=$!

sleep 2

if ! kill -0 "$BOOTSTRAP_PID" 2>/dev/null; then
    echo "FAIL: bootstrap exited immediately"
    cat "$OUT_LOG"
    exit 1
fi

RUNTIME_DIR="$(grep -o '/tmp/blockchain_[0-9]*' "$OUT_LOG" | head -n1)"
echo "runtime directory: ${RUNTIME_DIR:-<not found>}"

echo "--- status ---"
send_cmd "status" 1

phase "Phase 2: transactions and mining"
send_cmd 'submit "Alice pays Bob 10 coins"' 1
send_cmd 'submit "Charlie pays Dave 5 coins"' 1

echo "waiting for a miner to land a block (each attempt sleeps 1-5s, then rolls 1-in-$DIFFICULTY)..."

elapsed=0
mined=0
while [ "$elapsed" -lt "$MINE_TIMEOUT_SECONDS" ]; do
    response="$(send_cmd "request blockchain" 2)"
    block_count="$(printf '%s\n' "$response" | grep -c '^[0-9a-f]\{16\},')"

    if [ "$block_count" -gt 1 ]; then
        mined=1
        break
    fi

    elapsed=$((elapsed + 2))
    printf '.'
done
echo

if [ "$mined" -eq 0 ]; then
    echo "FAIL: no block was mined within ${MINE_TIMEOUT_SECONDS}s"
    send_cmd "stop" 1
    wait "$BOOTSTRAP_PID"
    exit 1
fi

echo "a block was mined, chain now has $block_count block(s)"
printf '%s\n' "$response"

phase "Phase 3: consensus queries"
genesis_hash="$(printf '%s\n' "$response" | awk -F',' '$1 == "0000000000000001" {print $3; exit}')"

echo "--- request block --index 1 ---"
send_cmd "request block --index 1" 1

if [ -n "$genesis_hash" ]; then
    echo "--- request block --hash $genesis_hash (this is block 0's own hash, read out of block 1's prev_hash field) ---"
    send_cmd "request block --hash $genesis_hash" 1
else
    echo "SKIP: could not read block 1's prev_hash field to look up genesis by hash"
fi

phase "Phase 4: pause, resume, and a simulated miner crash"
echo "--- pause ---"
send_cmd "pause" 1

echo "--- status (expect PAUSED) ---"
send_cmd "status" 1

echo "--- resume ---"
send_cmd "resume" 1

echo "--- status (expect RUNNING) ---"
status_output="$(send_cmd "status" 1)"
printf '%s\n' "$status_output"

miner_pid="$(printf '%s\n' "$status_output" | awk '$1 == "MINER" {print $3; exit}')"

if [ -n "$miner_pid" ]; then
    echo "killing miner pid $miner_pid to simulate a crash..."
    kill -9 "$miner_pid" 2>/dev/null

    echo "--- status (expect that miner TERMINATED, everything else still RUNNING) ---"
    send_cmd "status" 2
else
    echo "SKIP: could not read a miner PID out of the status table"
fi

phase "Phase 5: persistence and validation"
echo "--- save blockchain demo_output.csv ---"
send_cmd "save blockchain demo_output.csv" 1

if [ -f "$VERIFY_OUTPUT_CSV" ]; then
    echo "--- bash/blockchain.sh --verify demo_output.csv ---"
    (cd "$REPO_ROOT" && bash bash/blockchain.sh --verify demo_output.csv)
else
    echo "FAIL: demo_output.csv was not created"
fi

echo "--- bash/blockchain.sh --merkle (canned example, independent of the live chain) ---"
(cd "$REPO_ROOT" && bash bash/blockchain.sh --merkle "Alice pays Bob 10 coins::Charlie pays Dave 5 coins")

phase "Phase 6: shutdown"
echo "--- stop ---"
echo "stop" >&3

if ! wait "$BOOTSTRAP_PID"; then
    echo "FAIL: bootstrap did not shut down cleanly"
    cat "$OUT_LOG"
    exit 1
fi

echo "bootstrap exited cleanly"

if [ -n "$RUNTIME_DIR" ] && [ -d "$RUNTIME_DIR" ]; then
    echo "FAIL: runtime directory was not removed: $RUNTIME_DIR"
    exit 1
fi

echo "runtime directory removed"
echo
echo "Demo scenario complete."
