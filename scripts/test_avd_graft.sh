#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Graft-mode test runner for AVDs where self-built .ko gets rejected by the
# Android 15/16/17 GKI kernel's kCFI initcall typeid check (Pixel_35+).
#
# Instead of loading kernelhook.ko directly, this script injects the
# KernelHook payload into a vendor .ko that already has the kernel's expected
# kCFI hash on init_module, then loads the grafted binary and runs consumer
# modules (hello_hook et al.) on top.
#
# Usage:
#   ./scripts/test_avd_graft.sh Pixel_35                 # single AVD
#   ./scripts/test_avd_graft.sh Pixel_35 Pixel_36_1      # multiple
#   ./scripts/test_avd_graft.sh --keep-emulator Pixel_35 # reuse running AVD
#
# The script picks a vendor .ko host automatically from /system/lib/modules,
# filtering for (init_module present) AND (zero EXPORT_SYMBOL entries) so the
# kernel's CONFIG_MODULE_SIG_PROTECT check stays quiet.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KEEP_EMULATOR=0
KH_CONSUMERS="hello_hook"
AVDS=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --keep-emulator) KEEP_EMULATOR=1; shift ;;
        --consumers=*)   KH_CONSUMERS="${1#--consumers=}"; shift ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        -*) echo "unknown flag $1" >&2; exit 2 ;;
        *)  AVDS+=("$1"); shift ;;
    esac
done

if [ "${#AVDS[@]}" -eq 0 ]; then
    echo "Usage: $0 [--keep-emulator] AVD_NAME..." >&2
    exit 2
fi

. "$ROOT/scripts/lib/detect_toolchain.sh" || { echo "toolchain detect failed"; exit 1; }
. "$ROOT/scripts/lib/test_common.sh"

EMULATOR="$KH_ANDROID_SDK/emulator/emulator"
GRAFT="$ROOT/tools/kmod_loader/graft_vendor_ko"
LOADER="$ROOT/tools/kmod_loader/kmod_loader_arm64"
PAYLOAD="$ROOT/kmod/kh_payload.o"
NM="$KH_NDK_BIN/llvm-nm"

# Ensure build artifacts are present.
[ -x "$GRAFT" ] || { echo "Missing $GRAFT — run 'make -C tools/kmod_loader graft_vendor_ko'" >&2; exit 1; }

# Auto-build kh_payload.o if missing. test_avd_kmod.sh's `make module-dual`
# runs `make clean` which deletes this artifact, so a fresh kmod-then-graft
# run sequence used to break with "Missing kh_payload.o". Build it inline
# instead of failing.
if [ ! -f "$PAYLOAD" ]; then
    printf "${KH_BOLD}Building kh_payload.o (graft target)...${KH_RESET}\n"
    ( cd "$ROOT/kmod" && rm -f src/main.kmod.o && make payload ) \
        >/tmp/kh_graft_payload_build.log 2>&1 || {
            echo "kh_payload.o build failed — see /tmp/kh_graft_payload_build.log" >&2
            tail -20 /tmp/kh_graft_payload_build.log >&2
            exit 1
        }
fi
if [ ! -x "$LOADER" ]; then
    printf "${KH_BOLD}Building kmod_loader (aarch64)...${KH_RESET}\n"
    "$KH_CC" --target=aarch64-linux-android36 -DEMBED_PROBE_KO -DKH_RESOLVER_DEFINE_SPECS -static -O2 \
        -o "$LOADER" \
        "$ROOT/tools/kmod_loader/kmod_loader.c" \
        "$ROOT/tools/kmod_loader/devices_table.generated.c" \
        "$ROOT/tools/kmod_loader/resolver.c" \
        "$ROOT/tools/kmod_loader"/strategies/*.c \
        "$ROOT/tools/kmod_loader/subcommands.c" || { echo "loader build failed"; exit 1; }
fi

# Build consumers if missing.
IFS=',' read -ra CONSUMER_ARR <<< "$KH_CONSUMERS"
for c in "${CONSUMER_ARR[@]}"; do
    if [ ! -f "$ROOT/examples/$c/$c.ko" ]; then
        printf "${KH_BOLD}Building consumer %s...${KH_RESET}\n" "$c"
        ( cd "$ROOT/examples/$c" && make ) >/tmp/kh_graft_consumer_build.log 2>&1 || {
            echo "consumer $c build failed — see /tmp/kh_graft_consumer_build.log"; exit 1; }
    fi
done

consumer_marker() {
    case "$1" in
        hello_hook)     echo "hello_hook: hooked do_sys_open" ;;
        fp_hook)        echo "kh_fp_hook: function pointer hooked" ;;
        hook_chain)     echo "hook_chain: registered 3 before callbacks" ;;
        hook_wrap_args) echo "hook_wrap_args: hooked do_sys_open" ;;
        ksyms_lookup)   echo "ksyms_lookup: all lookups complete" ;;
        *)              echo "$1:" ;;
    esac
}

kill_all_emulators() {
    for s in $(adb devices 2>/dev/null | grep emulator- | awk '{print $1}'); do
        adb -s "$s" emu kill >/dev/null 2>&1 || true
    done
    sleep 3
    pkill -9 -f 'qemu-system' 2>/dev/null || true
    sleep 2
    for _ in $(seq 1 10); do
        adb devices 2>/dev/null | grep -q emulator- || break
        sleep 2
    done
}

start_avd() {
    local avd="$1"
    if [ "$KEEP_EMULATOR" -eq 1 ]; then
        # Scan ALL running emulators for one whose AVD name matches. Lets the
        # user run multiple AVDs in parallel (Pixel_31 on 5556, Pixel_35 on
        # 5558, etc.) and have us pick the right one rather than blindly
        # taking head -1 and falling through to kill_all_emulators.
        local s
        for s in $(adb devices 2>/dev/null | awk '/emulator-/{print $1}'); do
            local running_avd
            running_avd=$(adb -s "$s" emu avd name 2>/dev/null | head -1 | tr -d '\r')
            if [ "$running_avd" = "$avd" ]; then
                echo "$s"; return 0
            fi
        done
        # --keep-emulator was requested but no running AVD matches: do NOT
        # kill anything (would blow away unrelated emulators); just fail.
        echo "" ; return 1
    fi
    kill_all_emulators
    adb kill-server >/dev/null 2>&1 || true
    sleep 1
    adb start-server >/dev/null 2>&1
    screen -dmS "avdgraft_$avd" bash -c "'$EMULATOR' -avd '$avd' -no-window -no-audio -no-boot-anim -no-snapshot-load -gpu swiftshader_indirect -port 5556 >/tmp/kh_graft_emu_${avd}.log 2>&1"
    local booted=0
    for _ in $(seq 1 48); do
        sleep 5
        if adb devices 2>/dev/null | grep -q "^emulator-5556[[:space:]]\+device"; then
            if [ "$(adb -s emulator-5556 shell 'getprop sys.boot_completed' 2>/dev/null | tr -d '\r\n')" = "1" ]; then
                booted=1; break
            fi
        fi
    done
    if [ "$booted" -ne 1 ]; then
        echo "" ; return 1
    fi
    echo "emulator-5556"
}

pick_host_ko() {
    local serial="$1"
    local tmpd="$2"
    # Enumerate /system/lib/modules and pick the first that has init_module +
    # zero __ksymtab_ exports. Prefer the smallest so the grafted artifact
    # stays compact.
    adb -s "$serial" shell 'ls /system/lib/modules/' 2>/dev/null \
      | tr -d '\r' | grep '\.ko$' | sort > "$tmpd/mods.txt"
    local best=""
    local best_size=99999999
    while read -r m; do
        [ -z "$m" ] && continue
        adb -s "$serial" pull "/system/lib/modules/$m" "$tmpd/$m" >/dev/null 2>&1 || continue
        local sz
        sz=$(stat -f%z "$tmpd/$m" 2>/dev/null)
        [ -z "$sz" ] && continue
        [ "$sz" -gt 65536 ] && continue
        local has_init ksym
        has_init=$("$NM" "$tmpd/$m" 2>/dev/null | grep -cE '^[0-9a-f]+ T init_module$')
        ksym=$("$NM" "$tmpd/$m" 2>/dev/null | grep -cE ' __ksymtab_[A-Za-z]')
        if [ "$has_init" -eq 1 ] && [ "$ksym" -eq 0 ] && [ "$sz" -lt "$best_size" ]; then
            best="$m"
            best_size="$sz"
        fi
    done < "$tmpd/mods.txt"
    [ -n "$best" ] && echo "$tmpd/$best"
}

test_avd() {
    local avd="$1"
    printf "\n${KH_BOLD}======== Graft: %s ========${KH_RESET}\n" "$avd"
    local serial
    serial=$(start_avd "$avd")
    if [ -z "$serial" ]; then
        printf "  ${KH_RED}SKIP${KH_RESET} %s: boot timeout\n" "$avd"
        return 1
    fi
    adb -s "$serial" root >/dev/null 2>&1
    sleep 3
    adb -s "$serial" wait-for-device
    adb -s "$serial" shell 'setenforce 0; echo 0 > /proc/sys/kernel/kptr_restrict' >/dev/null 2>&1

    local kaddr
    kaddr=$(adb -s "$serial" shell 'grep "T kallsyms_lookup_name$" /proc/kallsyms' \
            2>/dev/null | awk '{print $1}' | head -1 | tr -d '\r')
    if [ -z "$kaddr" ] || [ "$kaddr" = "0000000000000000" ]; then
        printf "  ${KH_RED}FAIL${KH_RESET} %s: can't read kallsyms_lookup_name (kptr_restrict?)\n" "$avd"
        return 1
    fi

    local tmpd="/tmp/kh_graft_$avd"
    rm -rf "$tmpd"; mkdir -p "$tmpd"
    local host
    host=$(pick_host_ko "$serial" "$tmpd")
    if [ -z "$host" ]; then
        printf "  ${KH_RED}FAIL${KH_RESET} %s: no suitable host .ko found in /system/lib/modules\n" "$avd"
        return 1
    fi
    printf "  host: %s\n" "$(basename "$host")"
    printf "  kallsyms_addr: 0x%s\n" "$kaddr"

    "$GRAFT" --host "$host" --payload "$PAYLOAD" --out "$tmpd/grafted.ko" \
             --kallsyms-addr "0x$kaddr" >/tmp/kh_graft_tool.log 2>&1 || {
        printf "  ${KH_RED}FAIL${KH_RESET} %s: graft_vendor_ko exited non-zero\n" "$avd"
        tail -20 /tmp/kh_graft_tool.log | sed 's/^/    /'
        return 1
    }

    adb -s "$serial" push "$tmpd/grafted.ko" /data/local/tmp/grafted.ko >/dev/null 2>&1
    adb -s "$serial" push "$LOADER" /data/local/tmp/kmod_loader >/dev/null 2>&1
    adb -s "$serial" shell 'chmod +x /data/local/tmp/kmod_loader; dmesg -c >/dev/null' >/dev/null 2>&1

    # Stage 1: insmod grafted (carries kernelhook payload).
    local kh_out
    kh_out=$(adb -s "$serial" shell 'insmod /data/local/tmp/grafted.ko 2>&1; echo "rc=$?"' 2>&1 | tr -d '\r')
    if ! echo "$kh_out" | grep -q '^rc=0$'; then
        printf "  ${KH_RED}FAIL${KH_RESET} %s: grafted insmod failed\n" "$avd"
        echo "$kh_out" | sed 's/^/    /'
        adb -s "$serial" shell 'dmesg | tail -20' 2>/dev/null | sed 's/^/    /'
        return 1
    fi
    if ! adb -s "$serial" shell 'dmesg | grep "kernelhook: loaded successfully"' 2>/dev/null | grep -q loaded; then
        printf "  ${KH_RED}FAIL${KH_RESET} %s: no 'kernelhook: loaded successfully' in dmesg\n" "$avd"
        return 1
    fi
    printf "  ${KH_GREEN}OK${KH_RESET}   grafted kernelhook loaded\n"

    # Stage 2: per-consumer load + marker check.
    #
    # Same dmesg-eviction race as test_avd_kmod.sh: with multiple
    # hook-installing consumers, openat callbacks pr_info() faster than the
    # kernel ring buffer can hold (~256 KB cycles in <1 s once 4 hooks are
    # active). Run kmod_loader and grep for the marker in the SAME adb-shell
    # so the grep hits microseconds after finit_module returns. Currently
    # default KH_CONSUMERS="hello_hook" is single-consumer (no race), but
    # this keeps the test robust if --consumers expands the set later.
    local all_pass=1
    for c in "${CONSUMER_ARR[@]}"; do
        adb -s "$serial" push "$ROOT/examples/$c/$c.ko" "/data/local/tmp/$c.ko" >/dev/null 2>&1
        local marker
        marker=$(consumer_marker "$c")
        local _esc_marker
        _esc_marker=$(printf '%s' "$marker" | sed -e 's/[\\"`$]/\\&/g')
        local _out
        _out=$(adb -s "$serial" shell "
            /data/local/tmp/kmod_loader /data/local/tmp/$c.ko kallsyms_addr=0x$kaddr >/dev/null 2>&1
            echo __KH_LD_RC__\$?
            dmesg 2>/dev/null | grep -F \"$_esc_marker\" | tail -1
        " 2>&1 | tr -d '\r')
        local ld_rc
        ld_rc=$(echo "$_out" | grep -oE '__KH_LD_RC__[0-9]+' | head -1 | sed 's/__KH_LD_RC__//')
        local _race_marker_line
        _race_marker_line=$(echo "$_out" | awk '/__KH_LD_RC__/{found=1; next} found' | tail -1)
        if [ "$ld_rc" != "0" ]; then
            printf "  ${KH_RED}FAIL${KH_RESET} consumer %s load rc=%s\n" "$c" "${ld_rc:-?}"
            all_pass=0; continue
        fi
        if [ -n "$_race_marker_line" ]; then
            printf "  ${KH_GREEN}OK${KH_RESET}   %s (marker '%s')\n" "$c" "$marker"
            continue
        fi
        # Race-grep missed; fall back to a fresh dmesg sweep + KH/I prefix.
        sleep 2
        if adb -s "$serial" shell 'dmesg' 2>/dev/null | grep -Fq "$marker"; then
            printf "  ${KH_GREEN}OK${KH_RESET}   %s (marker '%s')\n" "$c" "$marker"
        elif adb -s "$serial" shell 'dmesg' 2>/dev/null | grep -Eq "\\[KH/I\\] $c:"; then
            printf "  ${KH_GREEN}OK${KH_RESET}   %s (KH/I marker fallback)\n" "$c"
        else
            printf "  ${KH_RED}FAIL${KH_RESET} %s: marker not seen\n" "$c"
            all_pass=0
        fi
    done

    if [ "$all_pass" -eq 1 ]; then
        printf "  ${KH_BOLD}${KH_GREEN}[PASS] %s${KH_RESET}\n" "$avd"
        return 0
    fi
    printf "  ${KH_BOLD}${KH_RED}[FAIL] %s${KH_RESET}\n" "$avd"
    return 1
}

FAIL_COUNT=0
for avd in "${AVDS[@]}"; do
    if ! test_avd "$avd"; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

if [ "$KEEP_EMULATOR" -eq 0 ]; then
    kill_all_emulators
fi

if [ "$FAIL_COUNT" -gt 0 ]; then
    printf "\n${KH_RED}${FAIL_COUNT} AVD(s) failed${KH_RESET}\n"
    exit 1
fi
printf "\n${KH_GREEN}All %d AVD(s) passed${KH_RESET}\n" "${#AVDS[@]}"
