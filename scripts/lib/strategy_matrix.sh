#!/usr/bin/env bash
# scripts/lib/strategy_matrix.sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Bash library for the strategy-matrix tooling (SP-7 Phase D, Tasks 25+26).
# Source this file; do not execute directly.
#
# Exported functions:
#   strategy_matrix_dump   <device_id> [adb_args...]
#   strategy_matrix_check  <device_id> [adb_args...]
#   strategy_matrix_accept <device_id> [adb_args...]
#
# All three forward extra arguments verbatim to adb (e.g. -s <serial>).

set -euo pipefail

_SM_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
_SM_GOLDEN_DIR="$_SM_ROOT/tests/golden/strategy_matrix"
_SM_EXPECT="$_SM_GOLDEN_DIR/expectations.yaml"

# ---------------------------------------------------------------------------
# strategy_matrix_dump <device_id> [adb_args...]
#
# Emit a YAML document to stdout describing the current state of the strategy
# registry on the connected device.  The document has three sections:
#   run_metadata  -- date, kernel uname, linux_banner sha256, git sha, device id
#   capabilities  -- parsed from /sys/kernel/debug/kernelhook/strategies
#   observed_values -- scalar values extracted from recent dmesg [KH/…] lines
# ---------------------------------------------------------------------------
strategy_matrix_dump() {
    local device="$1"; shift
    local -a adb_args=("$@")

    local uname banner_sha git_sha
    uname=$(adb "${adb_args[@]}" shell uname -r 2>/dev/null | tr -d '\r\n') \
        || uname="unavailable"
    banner_sha=$(adb "${adb_args[@]}" shell 'cat /proc/version 2>/dev/null' 2>/dev/null \
        | shasum -a 256 2>/dev/null | awk '{print $1}') \
        || banner_sha="unavailable"
    git_sha=$(cd "$_SM_ROOT" && git rev-parse --short HEAD 2>/dev/null || echo nogit)

    cat <<YAML
run_metadata:
  date: $(date -u +%Y-%m-%dT%H:%M:%SZ)
  kernel_uname: "$uname"
  linux_banner_sha256: "$banner_sha"
  kernelhook_git_sha: "$git_sha"
  device: "$device"

YAML

    # Parse /sys/kernel/debug/kernelhook/strategies.
    # Each line: <32-char-cap> <24-char-strat> prio=N enabled=N winner=Y|""
    # Convert to YAML capability blocks.
    local strat_dump
    strat_dump=$(adb "${adb_args[@]}" shell \
        'cat /sys/kernel/debug/kernelhook/strategies 2>/dev/null' 2>/dev/null \
        | tr -d '\r') || strat_dump=""

    echo "capabilities:"

    if [[ -z "$strat_dump" ]]; then
        echo "  # (no strategies dump available -- module not loaded or debugfs not mounted)"
    else
        echo "$strat_dump" | awk '
        NF < 2 { next }
        {
            cap   = $1
            strat = $2
            prio=""; enabled=""; winner="false"
            for (i = 3; i <= NF; i++) {
                n = split($i, kv, "=")
                if (n == 2) {
                    if (kv[1] == "prio")    prio    = kv[2]
                    if (kv[1] == "enabled") enabled = kv[2]
                    if (kv[1] == "winner")  winner  = (kv[2] == "Y" ? "true" : "false")
                }
            }
            if (cap != prev_cap) {
                if (prev_cap != "") printf "\n"
                printf "  %s:\n    strategies:\n", cap
                prev_cap = cap
            }
            printf "      - { name: %s, prio: %s, enabled: %s, winner: %s }\n",
                   strat, prio, enabled, winner
        }
        '
    fi

    # Dmesg-derived scalar values.  Kernels that load kernelhook.ko (or
    # kh_test.ko) log resolved values via pr_info at the [KH/<cap>] tag.
    # We tail the last 500 dmesg lines so we catch values from the most
    # recent module load without pulling the entire ring buffer.
    local dmesg_tail
    dmesg_tail=$(adb "${adb_args[@]}" shell \
        'dmesg 2>/dev/null | tail -500' 2>/dev/null \
        | tr -d '\r') || dmesg_tail=""

    # Helper: extract last occurrence of pattern from dmesg_tail, or "unknown".
    _sm_extract() {
        local pattern="$1"
        local val
        val=$(printf '%s\n' "$dmesg_tail" | grep -oE "$pattern" | tail -1 | grep -oE '[0-9a-fx]+$') \
            || val="unknown"
        [[ -n "$val" ]] || val="unknown"
        printf '%s' "$val"
    }

    local kimage_voffset memstart_addr swapper_pg_dir thread_size pt_regs_size
    kimage_voffset=$(_sm_extract 'kimage_voffset[= ]+0x[0-9a-f]+')
    memstart_addr=$(_sm_extract 'memstart_addr[= ]+0x[0-9a-f]+')
    swapper_pg_dir=$(_sm_extract 'pgd=0x[0-9a-f]+')
    thread_size=$(_sm_extract 'thread_size=[0-9]+')
    pt_regs_size=$(_sm_extract 'pt_regs_size=0x[0-9a-f]+')

    cat <<YAML

observed_values:
  kimage_voffset: "$kimage_voffset"
  memstart_addr:  "$memstart_addr"
  swapper_pg_dir: "$swapper_pg_dir"
  thread_size:    "$thread_size"
  pt_regs_size:   "$pt_regs_size"
YAML
}

# ---------------------------------------------------------------------------
# strategy_matrix_check <device_id> [adb_args...]
#
# Dump current state, compare against the stored golden for <device_id>, and
# run the Python expectation validator.  Exits 0 on full match, 1 on drift.
# ---------------------------------------------------------------------------
strategy_matrix_check() {
    local device="$1"; shift
    local -a adb_args=("$@")

    local current_yaml="/tmp/strategy_matrix_${device}_current.yaml"
    strategy_matrix_dump "$device" "${adb_args[@]}" > "$current_yaml"

    local golden="$_SM_GOLDEN_DIR/values/${device}.yaml"
    if [[ ! -f "$golden" ]]; then
        printf "No golden at %s -- run 'strategy-matrix --accept %s' to create.\n" \
               "$golden" "$device" >&2
        printf "\n--- Current dump ---\n" >&2
        cat "$current_yaml" >&2
        return 1
    fi

    # Strip run_metadata.date before diffing (it changes every run by design).
    local rc=0
    diff <(grep -v '^  date:' "$golden") \
         <(grep -v '^  date:' "$current_yaml") || rc=$?

    if [[ $rc -ne 0 ]]; then
        printf "DRIFT detected for device '%s'.\n" "$device" >&2
        return 1
    fi

    # Also run the Python validator against expectations.
    python3 "$_SM_ROOT/scripts/lib/strategy_matrix_check.py" \
            "$_SM_EXPECT" "$current_yaml"
}

# ---------------------------------------------------------------------------
# strategy_matrix_accept <device_id> [adb_args...]
#
# Overwrite the golden for <device_id> with a fresh dump, then regenerate
# survival.tsv across all existing goldens.
# ---------------------------------------------------------------------------
strategy_matrix_accept() {
    local device="$1"; shift
    local -a adb_args=("$@")

    local target="$_SM_GOLDEN_DIR/values/${device}.yaml"
    strategy_matrix_dump "$device" "${adb_args[@]}" > "$target"
    printf "Golden written: %s\n" "$target"

    _sm_regen_survival_tsv
}

# ---------------------------------------------------------------------------
# Internal: regenerate survival.tsv via the Python helper.
# ---------------------------------------------------------------------------
_sm_regen_survival_tsv() {
    python3 "$_SM_ROOT/scripts/lib/strategy_matrix_regen_tsv.py" \
            "$_SM_EXPECT" \
            "$_SM_GOLDEN_DIR/values"
}
