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
#   run_metadata     -- date, kernel uname, linux_banner sha256, git sha, device id
#   capabilities     -- parsed from `[kh_strategy] <cap> <strat> …` dmesg lines
#                       (emitted by kh_strategy_dump() called from kh_strategy_post_init
#                       and at the tail of kh_test.ko's test harness)
#   observed_values  -- scalar values extracted from recent dmesg [KH/…] lines
#
# Debugfs was removed to avoid exposing kCFI entry points (4 × file_operations
# callbacks) that would otherwise need per-kernel hash patching by kmod_loader.
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
# SPDX-License-Identifier: GPL-2.0-or-later
# Auto-generated via: scripts/test.sh strategy-matrix --accept $device
run_metadata:
  date: $(date -u +%Y-%m-%dT%H:%M:%SZ)
  kernel_uname: "$uname"
  linux_banner_sha256: "$banner_sha"
  kernelhook_git_sha: "$git_sha"
  device: "$device"

YAML

    # Grab the full dmesg once; subsequent parses slice it.
    local dmesg_full
    dmesg_full=$(adb "${adb_args[@]}" shell 'dmesg 2>/dev/null' 2>/dev/null \
        | tr -d '\r') || dmesg_full=""

    # Strategy snapshot: take the tail slice of `[kh_strategy] <cap> <strat>
    # prio=N enabled=N winner=Y|""` lines. The module emits a full snapshot
    # once at kh_strategy_post_init and once at the end of kh_test_init; we
    # want the most recent one, so locate its start by scanning backwards for
    # the first registered capability and keep from there.
    #
    # Each matching line is uniquely identifiable by the `[kh_strategy] `
    # prefix followed by a `prio=` field (that filters out the KALLSYMS
    # diagnostic and consistency-check notices which share the prefix but
    # lack `prio=`).
    local strat_lines
    strat_lines=$(printf '%s\n' "$dmesg_full" \
        | grep -E '\[kh_strategy\][^A-Z]*prio=' \
        | tail -200) || strat_lines=""

    echo "capabilities:"

    if [[ -z "$strat_lines" ]]; then
        echo "  # (no strategy snapshot in dmesg -- module not loaded?)"
    else
        # awk expects each line of form
        #   ...[kh_strategy] <cap> <strat> prio=N enabled=N winner=Y|""
        # Extract fields after the [kh_strategy] tag.
        printf '%s\n' "$strat_lines" | awk '
        {
            # Strip everything up to and including "[kh_strategy] ".
            idx = index($0, "[kh_strategy] ")
            if (idx == 0) next
            payload = substr($0, idx + length("[kh_strategy] "))
            n = split(payload, f, /[ \t]+/)
            if (n < 3) next
            cap = f[1]; strat = f[2]
            prio = ""; enabled = "false"; winner = "false"
            for (i = 3; i <= n; i++) {
                k = split(f[i], kv, "=")
                if (k < 1) continue
                if (kv[1] == "prio")    prio    = (k >= 2 ? kv[2] : "")
                if (kv[1] == "enabled") enabled = (k >= 2 && kv[2] == "1" ? "true" : "false")
                if (kv[1] == "winner")  winner  = (k >= 2 && kv[2] == "Y" ? "true" : "false")
            }
            if (prio == "") next
            # Collapse duplicate snapshot lines (older boot snapshot vs the
            # trailing one). Keep the LAST line per (cap,strat); accomplish
            # this by deferring output until END.
            key = cap "\t" strat
            order[key] = (order[key] == "" ? ++ord : order[key])
            caps[key] = cap
            strats[key] = strat
            prios[key] = prio
            ens[key] = enabled
            wins[key] = winner
            cap_first_order[cap] = (cap_first_order[cap] == "" ? ord : cap_first_order[cap])
        }
        END {
            # Emit capabilities in first-appearance order; within each cap,
            # strategies in prio order.
            n_keys = 0
            for (key in caps) { keys[++n_keys] = key }
            # Sort keys by (cap_first_order, prio).
            for (i = 1; i <= n_keys; i++) {
                for (j = i + 1; j <= n_keys; j++) {
                    ai = cap_first_order[caps[keys[i]]]
                    aj = cap_first_order[caps[keys[j]]]
                    if (ai > aj || (ai == aj && (prios[keys[i]] + 0) > (prios[keys[j]] + 0))) {
                        t = keys[i]; keys[i] = keys[j]; keys[j] = t
                    }
                }
            }
            prev_cap = ""
            for (i = 1; i <= n_keys; i++) {
                k = keys[i]
                if (caps[k] != prev_cap) {
                    if (prev_cap != "") printf "\n"
                    printf "  %s:\n    strategies:\n", caps[k]
                    prev_cap = caps[k]
                }
                printf "      - { name: %s, prio: %s, enabled: %s, winner: %s }\n",
                       strats[k], prios[k], ens[k], wins[k]
            }
        }
        '
    fi

    # Dmesg-derived scalar values.  Kernels that load kernelhook.ko (or
    # kh_test.ko) log resolved values via pr_info at the [KH/<cap>] tag.
    # Use the tail-500 slice so we catch the most recent module load.
    local dmesg_tail
    dmesg_tail=$(printf '%s\n' "$dmesg_full" | tail -500)

    # Helper: extract last occurrence of pattern from dmesg_tail, or "unknown".
    # Patterns must end with a VALUE capture group (either 0x<hex> or bare decimal);
    # the trailing grep picks exactly that tail token, preserving any 0x prefix.
    _sm_extract() {
        local pattern="$1"
        local val
        val=$(printf '%s\n' "$dmesg_tail" | grep -oE "$pattern" | tail -1 \
                | grep -oE '(0x[0-9a-f]+|[0-9]+)$') \
            || val="unknown"
        [[ -n "$val" ]] || val="unknown"
        printf '%s' "$val"
    }

    # dmesg formats (see src/arch/arm64/pgtable.c + src/uaccess.c):
    #   "pgtable: kimage_voffset value=0x<hex>"
    #   "pgtable: memstart_addr=0x<hex> (PHYS_OFFSET)"
    #   "pgtable: init ok, pgd=0x<hex> (...) voffset=0x<hex> ... levels=<dec>"
    #   "uaccess: pt_regs_size=0x<hex> thread_size=<dec> (cached)"
    local kimage_voffset memstart_addr thread_size pt_regs_size
    kimage_voffset=$(_sm_extract 'kimage_voffset value=0x[0-9a-f]+')
    memstart_addr=$(_sm_extract 'memstart_addr=0x[0-9a-f]+')
    thread_size=$(_sm_extract 'thread_size=[0-9]+')
    pt_regs_size=$(_sm_extract 'pt_regs_size=0x[0-9a-f]+')

    cat <<YAML

observed_values:
  kimage_voffset: "$kimage_voffset"
  memstart_addr:  "$memstart_addr"
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
