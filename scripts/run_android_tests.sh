#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Run KernelHook tests on an Android device or emulator via ADB.
#
# Usage:
#   ./scripts/run_android_tests.sh                    # auto-detect device/emulator
#   ./scripts/run_android_tests.sh --serial ABCD1234  # target specific device
#   ./scripts/run_android_tests.sh --device-only      # USB devices only
#   ./scripts/run_android_tests.sh --emulator-only    # emulators only
#   ./scripts/run_android_tests.sh --build-dir DIR    # custom build dir
#   ./scripts/run_android_tests.sh --kmod             # kernel module (stub)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Colors
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BOLD='\033[1m'
RESET='\033[0m'

# Defaults
BUILD_DIR="build_android"
SERIAL=""
FILTER=""  # "" = any, "device" = USB only, "emulator" = emulator only
KMOD=0

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --serial)      SERIAL="$2"; shift 2 ;;
        --device-only) FILTER="device"; shift ;;
        --emulator-only) FILTER="emulator"; shift ;;
        --build-dir)   BUILD_DIR="$2"; shift 2 ;;
        --kmod)        KMOD=1; shift ;;
        -h|--help)
            echo "Usage: $0 [--serial SN] [--device-only] [--emulator-only] [--build-dir DIR] [--kmod]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ---- Verify ADB ----

if ! command -v adb &>/dev/null; then
    printf "${RED}Error: adb not found in PATH${RESET}\n"
    printf "Install Android SDK Platform Tools or set PATH.\n"
    exit 1
fi

# ---- Device detection ----

detect_device() {
    local devices
    devices=$(adb devices 2>/dev/null | tail -n +2 | grep -v "^$" | awk '{print $1}')

    if [ -z "$devices" ]; then
        printf "${RED}Error: No Android devices/emulators found.${RESET}\n"
        printf "Connect a USB device or start an emulator, then retry.\n"
        exit 1
    fi

    if [ -n "$SERIAL" ]; then
        if echo "$devices" | grep -qx "$SERIAL"; then
            echo "$SERIAL"
            return
        fi
        printf "${RED}Error: Device %s not found.${RESET}\n" "$SERIAL"
        printf "Available devices:\n"
        echo "$devices" | sed 's/^/  /'
        exit 1
    fi

    # Filter by type
    local selected=""
    for dev in $devices; do
        if [ "$FILTER" = "device" ] && echo "$dev" | grep -q "^emulator-"; then
            continue
        fi
        if [ "$FILTER" = "emulator" ] && ! echo "$dev" | grep -q "^emulator-"; then
            continue
        fi
        selected="$dev"
        break
    done

    if [ -z "$selected" ]; then
        printf "${RED}Error: No matching device found (filter: %s).${RESET}\n" "${FILTER:-any}"
        printf "Available devices:\n"
        echo "$devices" | sed 's/^/  /'
        exit 1
    fi

    echo "$selected"
}

DEVICE=$(detect_device)
ADB="adb -s $DEVICE"

# Classify device type
if echo "$DEVICE" | grep -q "^emulator-"; then
    DEV_TYPE="emulator"
else
    DEV_TYPE="USB device"
fi

printf "${BOLD}KernelHook Android Test Runner${RESET}\n"
printf "  Target: %s (%s)\n" "$DEVICE" "$DEV_TYPE"

# ---- Check root ----

HAS_ROOT=0
if $ADB shell "su -c id" 2>/dev/null | grep -q "uid=0"; then
    HAS_ROOT=1
    printf "  Root: ${GREEN}available${RESET}\n"
else
    printf "  Root: ${YELLOW}not available (some tests may skip)${RESET}\n"
fi

# ---- Build if needed ----

if [ ! -d "$BUILD_DIR" ]; then
    printf "\n${BOLD}Building for Android...${RESET}\n"
    cmake -B "$BUILD_DIR" \
          -DCMAKE_TOOLCHAIN_FILE=cmake/android-arm64.cmake \
          -DCMAKE_BUILD_TYPE=Debug \
          2>&1 | tail -3
    cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" \
          2>&1 | tail -3
fi

# ---- Discover test binaries ----

TEST_DIR="$BUILD_DIR/tests"
if [ ! -d "$TEST_DIR" ]; then
    printf "${RED}Error: Test directory %s not found.${RESET}\n" "$TEST_DIR"
    exit 1
fi

TESTS=()
for bin in "$TEST_DIR"/test_*; do
    [ -f "$bin" ] && [ -x "$bin" ] && TESTS+=("$(basename "$bin")")
done

if [ ${#TESTS[@]} -eq 0 ]; then
    printf "${RED}Error: No test binaries found in %s${RESET}\n" "$TEST_DIR"
    exit 1
fi

printf "  Tests found: %d\n\n" "${#TESTS[@]}"

# ---- Push binaries ----

REMOTE_DIR="/data/local/tmp/kh_tests"
$ADB shell "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR" 2>/dev/null

printf "${BOLD}Pushing test binaries...${RESET}\n"
for t in "${TESTS[@]}"; do
    $ADB push "$TEST_DIR/$t" "$REMOTE_DIR/$t" >/dev/null 2>&1
    $ADB shell "chmod +x $REMOTE_DIR/$t" 2>/dev/null
done

# ---- Run tests ----

PASSED=0
FAILED=0
SKIPPED=0
FAILURES=""

# Helper: extract a leading integer before a keyword (portable, no -P required)
_parse_count() {
    # Usage: _parse_count "3 passed" "passed"  → prints "3"
    echo "$1" | grep -oE "[0-9]+ $2" | grep -oE "^[0-9]+" | head -1
}

run_test() {
    local name="$1"
    local cmd="$REMOTE_DIR/$name"

    if [ "$HAS_ROOT" -eq 1 ]; then
        cmd="su -c $cmd"
    fi

    local output
    local rc=0
    output=$($ADB shell "$cmd" 2>&1) || rc=$?

    # Parse output for pass/fail/skip counts (grep -oE is portable; avoids -P)
    local t_passed t_failed t_skipped
    t_passed=$(_parse_count "$output" "passed")
    t_failed=$(_parse_count "$output" "failed")
    t_skipped=$(_parse_count "$output" "skipped")

    # Fallback: if parsing yields nothing, use exit code
    if [ -z "$t_passed" ] && [ -z "$t_failed" ]; then
        if [ "$rc" -eq 0 ]; then
            t_passed=1; t_failed=0
        else
            t_passed=0; t_failed=1
        fi
    fi

    t_passed="${t_passed:-0}"
    t_failed="${t_failed:-0}"
    t_skipped="${t_skipped:-0}"

    PASSED=$((PASSED + t_passed))
    FAILED=$((FAILED + t_failed))
    SKIPPED=$((SKIPPED + t_skipped))

    if [ "${t_failed}" -gt 0 ] || [ "$rc" -ne 0 ]; then
        printf "  ${RED}FAIL${RESET} %s (%s passed, %s failed, %s skipped)\n" \
               "$name" "$t_passed" "$t_failed" "$t_skipped"
        FAILURES="${FAILURES}\n  ${RED}FAIL${RESET} ${name}"
        echo "$output" | sed 's/^/       /'
    else
        printf "  ${GREEN}PASS${RESET} %s (%s passed, %s skipped)\n" \
               "$name" "$t_passed" "$t_skipped"
    fi
}

printf "${BOLD}Running tests on %s...${RESET}\n" "$DEVICE"
for t in "${TESTS[@]}"; do
    run_test "$t"
done

# ---- Kernel module stub ----

if [ "$KMOD" -eq 1 ]; then
    printf "\n${YELLOW}Kernel module testing not yet supported.${RESET}\n"
    printf "Future: push .ko → insmod → parse dmesg → rmmod\n"
fi

# ---- Cleanup ----

$ADB shell "rm -rf $REMOTE_DIR" 2>/dev/null

# ---- Summary ----

printf "\n${BOLD}========== Summary ==========${RESET}\n"
TOTAL=$((PASSED + FAILED + SKIPPED))
printf "  Total: %d  |  ${GREEN}Passed: %d${RESET}  |  ${RED}Failed: %d${RESET}  |  ${YELLOW}Skipped: %d${RESET}\n" \
    "$TOTAL" "$PASSED" "$FAILED" "$SKIPPED"

if [ -n "$FAILURES" ]; then
    printf "\nFailures:${FAILURES}\n"
fi

exit $((FAILED > 0 ? 1 : 0))
