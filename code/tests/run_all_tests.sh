#!/usr/bin/env bash

set -u

failed=0

for test_file in tests/test_*.sh; do
    printf 'Running %s...\n' "$test_file"

    if ! bash "$test_file"; then
        failed=1
    fi
done

exit "$failed"
