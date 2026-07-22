#!/usr/bin/env bash
# Removes sockets, runtime directories, generated CSV files, and
# stale processes.
#
# Best-effort cleanup for leftover /tmp/blockchain_<pid>/ runtime
# directories from a bootstrap run that crashed or was force-killed rather
# than shut down cleanly via 'stop' (which already removes its own runtime
# directory on a normal exit). Safe to run any time, including when there
# is nothing to clean up.
set -u

SHUTDOWN_WAIT_SECONDS=2
found_any=0

terminate_pid() {
    local pid="$1"
    local label="$2"

    kill -TERM "$pid" 2>/dev/null
    sleep "$SHUTDOWN_WAIT_SECONDS"

    if kill -0 "$pid" 2>/dev/null; then
        kill -KILL "$pid" 2>/dev/null
        echo "  killed $label (pid $pid, did not exit after SIGTERM)"
    else
        echo "  terminated $label (pid $pid)"
    fi
}

for runtime_dir in /tmp/blockchain_*; do
    [ -d "$runtime_dir" ] || continue

    found_any=1
    echo "found runtime directory: $runtime_dir"

    bootstrap_pid="${runtime_dir##*/blockchain_}"

    if [[ "$bootstrap_pid" =~ ^[0-9]+$ ]] && kill -0 "$bootstrap_pid" 2>/dev/null; then
        process_name="$(ps -p "$bootstrap_pid" -o comm= 2>/dev/null || true)"

        if [[ "$process_name" == *blockchain* ]]; then
            terminate_pid "$bootstrap_pid" "bootstrap"
        fi
    fi

    child_pids="$(pgrep -f "$runtime_dir" 2>/dev/null || true)"

    for pid in $child_pids; do
        terminate_pid "$pid" "child process"
    done

    if rm -rf "$runtime_dir"; then
        echo "  removed $runtime_dir"
    else
        echo "  FAIL: could not remove $runtime_dir"
    fi
done

if [ "$found_any" -eq 0 ]; then
    echo "nothing to clean up: no /tmp/blockchain_* runtime directories found"
fi
