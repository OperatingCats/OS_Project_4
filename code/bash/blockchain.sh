#!/usr/bin/env bash

set -uo pipefail

readonly SHA256_HEX_LENGTH=64
readonly U64_HEX_LENGTH=16

usage() {
    cat >&2 <<EOF
Usage:
  $0 --verify <state.csv>
  $0 --hash <block_hex>
  $0 --merkle <transaction1::transaction2::...>
EOF
}

sha256_string() {
    local value="${1-}"

    printf '%s' "$value" |
        sha256sum |
        awk '{print $1}'
}

is_u64_hex() {
    local value="${1-}"

    [[ ${#value} -eq $U64_HEX_LENGTH &&
       "$value" =~ ^[0-9a-f]{16}$ ]]
}

is_hash_hex() {
    local value="${1-}"

    [[ ${#value} -eq $SHA256_HEX_LENGTH &&
       "$value" =~ ^[0-9a-f]{64}$ ]]
}

is_valid_transaction() {
    local transaction="${1-}"

    [[ "$transaction" =~ ^[[:alnum:]]+\ pays\ [[:alnum:]]+\ [1-9][0-9]*\ coins$ ]]
}

split_transactions() {
    local input="${1-}"
    local -n output_array="$2"
    local remaining="$input"
    local transaction

    output_array=()

    [[ -n "$remaining" ]] || return 1

    while [[ "$remaining" == *"::"* ]]; do
        transaction="${remaining%%::*}"

        [[ -n "$transaction" ]] || return 1

        output_array+=("$transaction")
        remaining="${remaining#*::}"
    done

    [[ -n "$remaining" ]] || return 1
    output_array+=("$remaining")

    return 0
}

calculate_merkle_root() {
    local transaction_input="${1-}"
    local -a transactions=()
    local -a current_level=()
    local -a next_level=()
    local transaction
    local index
    local combined

    if ! split_transactions "$transaction_input" transactions; then
        echo "ERROR: invalid or empty transaction list" >&2
        return 1
    fi

    for transaction in "${transactions[@]}"; do
        current_level+=("$(sha256_string "$transaction")")
    done

    while true; do
        if (( ${#current_level[@]} % 2 != 0 )); then
            current_level+=("$(sha256_string "")")
        fi

        next_level=()

        for ((index = 0;
            index < ${#current_level[@]};
            index += 2)); do
            combined="${current_level[index]}${current_level[index + 1]}"
            next_level+=("$(sha256_string "$combined")")
        done

        current_level=("${next_level[@]}")

        if (( ${#current_level[@]} == 1 )); then
            break
        fi
    done

    printf '%s\n' "${current_level[0]}"
}

hash_block_fields() {
    local index="$1"
    local timestamp="$2"
    local previous_hash="$3"
    local merkle_root="$4"
    local nonce="$5"
    local transactions="$6"

    sha256_string \
        "${index}${timestamp}${previous_hash}${merkle_root}${nonce}${transactions}"
}

hash_hex_block() {
    local block_hex="${1-}"

    if [[ -z "$block_hex" ||
          ${#block_hex} -eq 0 ||
          $(( ${#block_hex} % 2 )) -ne 0 ||
          ! "$block_hex" =~ ^[0-9A-Fa-f]+$ ]]; then
        echo "ERROR: block_hex must contain an even number of hexadecimal characters" >&2
        return 1
    fi

    if ! command -v xxd >/dev/null 2>&1; then
        echo "ERROR: xxd is required for hexadecimal decoding" >&2
        return 1
    fi

    printf '%s' "$block_hex" |
        xxd -r -p |
        sha256sum |
        awk '{print $1}'
}

verify_blockchain() {
    local csv_path="${1-}"
    local expected_header
    local header
    local line
    local line_number=1

    local index
    local timestamp
    local previous_hash
    local merkle_root
    local nonce
    local transactions
    local extra

    local expected_index=0
    local calculated_merkle
    local calculated_previous_hash=""
    local block_count=0

    local previous_index=""
    local previous_timestamp=""
    local previous_previous_hash=""
    local previous_merkle_root=""
    local previous_nonce=""
    local previous_transactions=""

    local -a parsed_transactions=()
    local transaction

    expected_header="index,timestamp,prev_hash,merkle_root,nonce,transactions"

    if [[ -z "$csv_path" ]]; then
        echo "ERROR: missing CSV path" >&2
        return 1
    fi

    if [[ ! -f "$csv_path" ]]; then
        echo "ERROR: file not found: $csv_path" >&2
        return 1
    fi

    if [[ ! -r "$csv_path" ]]; then
        echo "ERROR: file is not readable: $csv_path" >&2
        return 1
    fi

    if ! IFS= read -r header < "$csv_path"; then
        echo "ERROR: blockchain CSV is empty" >&2
        return 1
    fi

    header="${header%$'\r'}"

    if [[ "$header" != "$expected_header" ]]; then
        echo "ERROR: invalid CSV header" >&2
        return 1
    fi

    while IFS= read -r line || [[ -n "$line" ]]; do
        ((line_number++))
        line="${line%$'\r'}"

        [[ -n "$line" ]] || continue

        index=""
        timestamp=""
        previous_hash=""
        merkle_root=""
        nonce=""
        transactions=""
        extra=""

        IFS=',' read -r \
            index \
            timestamp \
            previous_hash \
            merkle_root \
            nonce \
            transactions \
            extra <<< "$line"

        if [[ -n "$extra" ||
              -z "$index" ||
              -z "$timestamp" ||
              -z "$previous_hash" ||
              -z "$merkle_root" ||
              -z "$nonce" ||
              -z "$transactions" ]]; then
            echo "ERROR line $line_number: invalid number of CSV fields" >&2
            return 1
        fi

        if [[ "$transactions" != \"*\" ]]; then
            echo "ERROR line $line_number: transactions field must be quoted" >&2
            return 1
        fi

        transactions="${transactions#\"}"
        transactions="${transactions%\"}"

        if ! is_u64_hex "$index"; then
            echo "ERROR line $line_number: invalid index encoding" >&2
            return 1
        fi

        if ! is_u64_hex "$timestamp"; then
            echo "ERROR line $line_number: invalid timestamp encoding" >&2
            return 1
        fi

        if ! is_hash_hex "$previous_hash"; then
            echo "ERROR line $line_number: invalid previous hash" >&2
            return 1
        fi

        if ! is_hash_hex "$merkle_root"; then
            echo "ERROR line $line_number: invalid Merkle root encoding" >&2
            return 1
        fi

        if ! is_u64_hex "$nonce"; then
            echo "ERROR line $line_number: invalid nonce encoding" >&2
            return 1
        fi

        if (( 16#$index != expected_index )); then
            echo "ERROR line $line_number: wrong block index; expected $expected_index" >&2
            return 1
        fi

        if ! split_transactions "$transactions" parsed_transactions; then
            echo "ERROR line $line_number: invalid transaction list" >&2
            return 1
        fi

        # Genesis transaction is explicitly exempt from the normal regex.
        if (( expected_index > 0 )); then
            for transaction in "${parsed_transactions[@]}"; do
                if ! is_valid_transaction "$transaction"; then
                    echo "ERROR line $line_number: invalid transaction: $transaction" >&2
                    return 1
                fi
            done
        fi

        calculated_merkle="$(calculate_merkle_root "$transactions")" || return 1

        if [[ "$calculated_merkle" != "$merkle_root" ]]; then
            echo "ERROR line $line_number: invalid Merkle root" >&2
            return 1
        fi

        if (( expected_index > 0 )); then
            calculated_previous_hash="$(
                hash_block_fields \
                    "$previous_index" \
                    "$previous_timestamp" \
                    "$previous_previous_hash" \
                    "$previous_merkle_root" \
                    "$previous_nonce" \
                    "$previous_transactions"
            )"

            if [[ "$previous_hash" != "$calculated_previous_hash" ]]; then
                echo "ERROR line $line_number: broken previous-hash link" >&2
                return 1
            fi
        fi

        previous_index="$index"
        previous_timestamp="$timestamp"
        previous_previous_hash="$previous_hash"
        previous_merkle_root="$merkle_root"
        previous_nonce="$nonce"
        previous_transactions="$transactions"

        ((expected_index++))
        ((block_count++))
    done < <(tail -n +2 "$csv_path")

    if (( block_count == 0 )); then
        echo "ERROR: blockchain contains only the header and no blocks" >&2
        return 1
    fi

    echo "OK: blockchain is valid ($block_count blocks)"
}

main() {
    if (( $# != 2 )); then
        usage
        return 1
    fi

    case "$1" in
        --verify)
            verify_blockchain "$2"
            ;;
        --hash)
            hash_hex_block "$2"
            ;;
        --merkle)
            calculate_merkle_root "$2"
            ;;
        *)
            usage
            return 1
            ;;
    esac
}

main "$@"