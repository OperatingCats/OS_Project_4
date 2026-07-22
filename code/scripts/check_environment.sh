#!/usr/bin/env bash
# Checks required dependencies (gcc, make, bash, sha256sum, openssl).
set -u

failed=0

check() {
    local tool="$1"
    local hint="$2"

    if command -v "$tool" >/dev/null 2>&1; then
        printf 'PASS: %-10s (%s)\n' "$tool" "$(command -v "$tool")"
    else
        printf 'FAIL: %-10s not found%s\n' "$tool" "${hint:+ ($hint)}"
        failed=1
    fi
}

check gcc ""
check make ""
check bash ""
check sha256sum "part of coreutils"
check openssl "libssl-dev / openssl-devel"

echo

if [ "$failed" -eq 0 ]; then
    echo "All required dependencies are present."
else
    echo "One or more required dependencies are missing, see FAIL lines above."
fi

exit "$failed"
