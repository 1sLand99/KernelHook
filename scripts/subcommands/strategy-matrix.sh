#!/usr/bin/env bash
# scripts/subcommands/strategy-matrix.sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Strategy-matrix subcommand for scripts/test.sh.
#
# Usage (via scripts/test.sh):
#   scripts/test.sh strategy-matrix                     # check every known golden
#   scripts/test.sh strategy-matrix <device>            # check one device
#   scripts/test.sh strategy-matrix --dump <device>     # print fresh YAML to stdout
#   scripts/test.sh strategy-matrix --accept <device>   # overwrite golden + regen tsv
#   scripts/test.sh strategy-matrix --help              # show this help
#
# <device> may be:
#   Pixel_35          -> emulator-5554  (assumes the AVD is already booted)
#   emulator-5554     -> forwarded verbatim as -s emulator-5554
#   1B101FDF6003PM    -> forwarded verbatim as -s 1B101FDF6003PM (physical device)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=../lib/strategy_matrix.sh
source "$ROOT/scripts/lib/strategy_matrix.sh"
# shellcheck source=../lib/test_common.sh
source "$ROOT/scripts/lib/test_common.sh"

# ---------------------------------------------------------------------------
usage() {
    cat <<'EOF'
strategy-matrix subcommand

Usage:
  scripts/test.sh strategy-matrix                     Check every known golden
  scripts/test.sh strategy-matrix <device>            Check one device's golden
  scripts/test.sh strategy-matrix --dump <device>     Print fresh YAML to stdout
  scripts/test.sh strategy-matrix --accept <device>   Write/overwrite golden + regen tsv
  scripts/test.sh strategy-matrix --help              Show this help

<device> examples:
  Pixel_35          AVD name (mapped to emulator-5554; AVD must be booted)
  emulator-5554     adb serial for a running emulator
  1B101FDF6003PM    adb serial for a physical USB device

Notes:
  - 'check' diffs against tests/golden/strategy_matrix/values/<device>.yaml.
  - Run --accept once per device to create the initial golden.
  - Run --accept again after an intentional kernel upgrade to refresh.
  - Goldens exclude the run_metadata.date field from diffs (it always changes).
  - The Python validator (strategy_matrix_check.py) enforces expectations.yaml rules.
EOF
}

# ---------------------------------------------------------------------------
# Set the global _sm_adb array to the adb -s args appropriate for <device>.
# Avoids bash-4 nameref (macOS ships bash 3.2).
# ---------------------------------------------------------------------------
_set_adb_args() {
    local device="$1"
    _sm_adb=()
    case "$device" in
        Pixel_*)    _sm_adb=(-s emulator-5554) ;;
        *)          _sm_adb=(-s "$device")     ;;
    esac
}

# ---------------------------------------------------------------------------
MODE=check
DEVICE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dump)           MODE=dump;   shift ;;
        --accept)         MODE=accept; shift ;;
        --help|-h)        usage; exit 0 ;;
        -*)               printf "unknown option: %s\n" "$1" >&2; usage >&2; exit 2 ;;
        *)                DEVICE="$1"; shift ;;
    esac
done

# ---------------------------------------------------------------------------
GOLDEN_DIR="$ROOT/tests/golden/strategy_matrix"

_sm_adb=()   # global array used below (no 'local' at script scope)

case "$MODE" in
    dump)
        if [[ -z "$DEVICE" ]]; then
            printf "ERROR: --dump requires a device argument.\n" >&2
            usage >&2
            exit 2
        fi
        _set_adb_args "$DEVICE" _sm_adb
        strategy_matrix_dump "$DEVICE" "${_sm_adb[@]}"
        ;;

    accept)
        if [[ -z "$DEVICE" ]]; then
            printf "ERROR: --accept requires a device argument.\n" >&2
            usage >&2
            exit 2
        fi
        kh_section_start "strategy-matrix --accept $DEVICE"
        _set_adb_args "$DEVICE" _sm_adb
        strategy_matrix_accept "$DEVICE" "${_sm_adb[@]}"
        kh_section_end "strategy-matrix --accept $DEVICE" PASS
        ;;

    check)
        if [[ -n "$DEVICE" ]]; then
            # Check one device.
            kh_section_start "strategy-matrix check $DEVICE"
            _set_adb_args "$DEVICE" _sm_adb
            if strategy_matrix_check "$DEVICE" "${_sm_adb[@]}"; then
                kh_section_end "strategy-matrix check $DEVICE" PASS
                exit 0
            else
                kh_section_end "strategy-matrix check $DEVICE" FAIL
                exit 1
            fi
        else
            # Check every existing golden.
            _sm_rc=0; _sm_pass=0; _sm_fail=0; _sm_skip=0
            shopt -s nullglob
            _sm_golden_files=("$GOLDEN_DIR/values/"*.yaml)
            shopt -u nullglob

            if [[ ${#_sm_golden_files[@]} -eq 0 ]]; then
                printf "No goldens found in %s/values/ -- " "$GOLDEN_DIR"
                printf "run 'strategy-matrix --accept <device>' to create one.\n"
                # Not a hard failure: no goldens is expected before Task 29.
                exit 0
            fi

            for _sm_f in "${_sm_golden_files[@]}"; do
                _sm_dev=$(basename "$_sm_f" .yaml)
                kh_section_start "strategy-matrix check $_sm_dev"
                _set_adb_args "$_sm_dev" _sm_adb
                # Skip cleanly if no adb device corresponds to this golden
                # (CI paths run strategy-matrix after avd shutdown).
                if ! adb "${_sm_adb[@]}" get-state >/dev/null 2>&1; then
                    printf "  SKIP: no adb device for '%s'\n" "$_sm_dev"
                    kh_section_end "strategy-matrix check $_sm_dev" SKIP
                    _sm_skip=$((_sm_skip+1))
                    continue
                fi
                if strategy_matrix_check "$_sm_dev" "${_sm_adb[@]}" 2>&1; then
                    kh_section_end "strategy-matrix check $_sm_dev" PASS
                    _sm_pass=$((_sm_pass+1))
                else
                    kh_section_end "strategy-matrix check $_sm_dev" FAIL
                    _sm_fail=$((_sm_fail+1))
                    _sm_rc=1
                fi
            done

            kh_summary_line "$_sm_pass" "$_sm_fail" "$_sm_skip"
            exit "$_sm_rc"
        fi
        ;;
esac
