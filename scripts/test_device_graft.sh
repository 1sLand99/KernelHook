#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Graft-mode test runner for a physical (USB) Android device. Mirrors
# scripts/test_avd_graft.sh but skips emulator boot, routes privileged
# commands through `su -c` (Magisk-rooted), and pulls the host vendor .ko
# from /vendor/lib/modules (GKI partition layout) instead of
# /system/lib/modules (Cuttlefish/AVD layout).
#
# Use this when the device's GKI kernel rejects a self-built kernelhook.ko
# via kCFI initcall typeid mismatch (Pixel_35+ behavior, also observed on
# Pixel 6 6.1.99-android14-11 sub-version backports).
#
# Usage:
#   ./scripts/test_device_graft.sh                  # first connected non-emulator device
#   ./scripts/test_device_graft.sh <adb-serial>     # explicit device
#   ./scripts/test_device_graft.sh --consumers=hello_hook,fp_hook <serial>
#
# Prereqs:
#   - adb sees the device as "device"
#   - `su -c id` returns uid=0 (Magisk granted)
#   - SELinux can be set to Permissive (we toggle automatically)
#   - kptr_restrict can be turned off (we toggle automatically)
#   - CONFIG_MODULES=y, /proc/sys/kernel/modules_disabled=0

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KH_CONSUMERS="hello_hook"
KH_SERIAL=""

while [ "$#" -gt 0 ]; do
    case "$1" in
        --consumers=*) KH_CONSUMERS="${1#--consumers=}"; shift ;;
        -h|--help)
            sed -n '2,28p' "$0"
            exit 0
            ;;
        -*) echo "unknown flag $1" >&2; exit 2 ;;
        *)  KH_SERIAL="$1"; shift ;;
    esac
done

. "$ROOT/scripts/lib/detect_toolchain.sh" || { echo "toolchain detect failed" >&2; exit 1; }
. "$ROOT/scripts/lib/test_common.sh"

case "${KH_TOOLCHAIN_KIND:-}" in
    sys-gcc|sys-clang)
        echo "ERROR: test_device_graft.sh requires a real NDK (bionic ABI for kmod_loader)." >&2
        echo "       Set \$ANDROID_NDK_ROOT or install the NDK." >&2
        exit 2
        ;;
esac

GRAFT="$ROOT/tools/kmod_loader/graft_vendor_ko"
LOADER="$ROOT/tools/kmod_loader/kmod_loader_arm64"
PAYLOAD="$ROOT/kmod/kh_payload.o"
NM="$KH_NDK_BIN/llvm-nm"

[ -x "$GRAFT" ] || { echo "Missing $GRAFT — run 'make -C tools/kmod_loader graft_vendor_ko'" >&2; exit 1; }

# Auto-build kh_payload.o if missing — kmod-then-graft sequences run
# `make clean` for the kmod build which deletes this artifact, same as
# test_avd_graft.sh handles inline.
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
        "$ROOT/tools/kmod_loader/subcommands.c" || { echo "loader build failed" >&2; exit 1; }
fi

IFS=',' read -ra CONSUMER_ARR <<< "$KH_CONSUMERS"
for c in "${CONSUMER_ARR[@]}"; do
    if [ ! -f "$ROOT/examples/$c/$c.ko" ]; then
        printf "${KH_BOLD}Building consumer %s...${KH_RESET}\n" "$c"
        ( cd "$ROOT/examples/$c" && make ) >/tmp/kh_graft_consumer_build.log 2>&1 || {
            echo "consumer $c build failed — see /tmp/kh_graft_consumer_build.log" >&2; exit 1; }
    fi
done

# Pick device serial: explicit arg > first non-emulator `adb devices`.
SERIAL="${KH_SERIAL:-}"
if [ -z "$SERIAL" ]; then
    SERIAL=$(adb devices 2>/dev/null \
        | awk 'NR>1 && $2=="device" && $1 !~ /^emulator-/ {print $1; exit}')
    if [ -z "$SERIAL" ]; then
        echo "ERROR: no non-emulator adb device found. Run 'adb devices' and pass serial explicitly." >&2
        exit 2
    fi
fi
ADB="adb -s $SERIAL"

# Helper: run a shell command as root on the device. Same wrapper as
# test_device_kmod.sh — keeps the test surface flat.
dsu() { $ADB shell "su -c 'sh -c \"$*\"'"; }

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

printf "${KH_BOLD}KernelHook graft Device Test${KH_RESET}\n"
printf "Device serial: %s\n" "$SERIAL"
printf "Toolchain: %s\n" "$KH_TOOLCHAIN_DESC"

# Preflight: root + module loading enabled.
if ! ROOT_ID=$($ADB shell 'su -c id' 2>&1) || ! echo "$ROOT_ID" | grep -q 'uid=0'; then
    echo "ERROR: 'su -c id' did not return uid=0. Is Magisk root granted for adb shell?" >&2
    echo "  got: $ROOT_ID" >&2
    exit 2
fi
SELINUX=$(dsu "getenforce" 2>&1 | tr -d '[:space:]')
MD=$(dsu "cat /proc/sys/kernel/modules_disabled" 2>&1 | tr -d '[:space:]')
printf "  SELinux: %s   modules_disabled: %s\n" "$SELINUX" "$MD"
if [ "$MD" != "0" ]; then
    echo "ERROR: kernel has modules_disabled=1 — module loading is permanently blocked." >&2
    exit 2
fi
if [ "$SELINUX" = "Enforcing" ]; then
    printf "  ${KH_YELLOW}WARN${KH_RESET} SELinux is Enforcing — attempting setenforce 0 to avoid avc denials.\n"
    dsu "setenforce 0 2>&1 || true" >/dev/null || true
fi
# Keep the screen on while USB-tethered (prevents adb tunnel drop on Pixel
# during the test) — same fix as test_device_kmod.sh.
$ADB shell svc power stayon usb >/dev/null 2>&1 || true
$ADB shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true

UNAME=$($ADB shell 'uname -r' 2>/dev/null | tr -d '[:space:]')
SDK=$($ADB shell 'getprop ro.build.version.sdk' 2>/dev/null | tr -d '[:space:]')
printf "  API: %s   Kernel: %s\n" "$SDK" "$UNAME"

dsu "echo 0 > /proc/sys/kernel/kptr_restrict" >/dev/null 2>&1 || true
KADDR=$(dsu "cat /proc/kallsyms" 2>/dev/null \
    | grep -E ' [Tt] kallsyms_lookup_name$' | awk '{print $1}' | head -1)
if [ -z "$KADDR" ] || [ "$KADDR" = "0000000000000000" ]; then
    echo "ERROR: cannot read kallsyms_lookup_name (kptr_restrict?)" >&2
    exit 2
fi
printf "  kallsyms_addr: 0x%s\n" "$KADDR"

# pick_host_ko: same constraints as the AVD version (init_module present,
# zero __ksymtab exports, smallest size) but pulls from /vendor/lib/modules
# — GKI partition layout. AVD images put vendor .ko in /system/lib/modules;
# real Pixel devices put them in /vendor/lib/modules (and may leave
# /system/lib/modules empty entirely). Fall back to /system/lib/modules so
# this script also works on AVD-like images.
#
# Size cap is 256KB (vs. 64KB on AVD): Pixel vendor modules are larger
# than the toy modules in AVD images. The grafted output retains the
# host's relocations + sections, so smaller is still better, but 113KB
# 8021q.ko + 30KB payload is fine; the kernel module loader has no
# meaningful upper limit.
pick_host_ko() {
    local tmpd="$1"
    # Snapshot loaded modules — Pixel boot auto-loads ~95% of vendor .ko
    # files. Re-insmod'ing one fails with "File exists" before the kernel
    # ever processes our grafted payload. Filter to modules whose internal
    # name (basename without .ko) is NOT in /proc/modules.
    $ADB shell "su -c 'cat /proc/modules'" 2>/dev/null \
        | tr -d '\r' | awk '{print $1}' | sort > "$tmpd/loaded_mods.txt"
    # Search order matches tools/kmod_loader/kmod_loader.c's vendor-ko probe.
    # /vendor_dlkm is where Android 14+ devices using DLKM (Dynamic Loadable
    # Kernel Modules) keep vendor modules; /odm is per-OEM customization;
    # /lib/modules is the legacy GKI-on-host layout. AVD images put them
    # in /system/lib/modules, real Pixel uses /vendor/lib/modules.
    local mod_dir
    for mod_dir in /vendor_dlkm/lib/modules /vendor/lib/modules /system/lib/modules /odm/lib/modules /lib/modules; do
        local listing
        listing=$($ADB shell "su -c 'ls $mod_dir 2>/dev/null'" 2>/dev/null \
                  | tr -d '\r' | grep '\.ko$' | sort)
        [ -z "$listing" ] && continue
        echo "$listing" > "$tmpd/mods.txt"
        local best="" best_size=99999999
        local m
        while read -r m; do
            [ -z "$m" ] && continue
            local mod_name="${m%.ko}"
            # Skip modules that are already loaded — kernel module names
            # use - and _ interchangeably (Linux module loader normalizes
            # them), so check both forms before deciding the candidate
            # is unloaded.
            local mod_name_alt="${mod_name//-/_}"
            if grep -qx -- "$mod_name" "$tmpd/loaded_mods.txt" 2>/dev/null \
               || grep -qx -- "$mod_name_alt" "$tmpd/loaded_mods.txt" 2>/dev/null; then
                continue
            fi
            # `adb pull` works on Pixel production builds for
            # /vendor/lib/modules (world-readable), is much faster than
            # the `su -c cat` round-trip, and doesn't suffer from the
            # adb-shell stdout corruption seen with cat-of-binary.
            $ADB pull "$mod_dir/$m" "$tmpd/$m" >/dev/null 2>&1 || continue
            [ ! -s "$tmpd/$m" ] && { rm -f "$tmpd/$m"; continue; }
            local sz
            sz=$(stat -f%z "$tmpd/$m" 2>/dev/null)
            [ -z "$sz" ] && continue
            [ "$sz" -gt 262144 ] && continue
            local has_init has_exit ksym
            local nm_out
            nm_out=$("$NM" "$tmpd/$m" 2>/dev/null)
            has_init=$(echo "$nm_out" | grep -cE '^[0-9a-f]+ T init_module$')
            # graft_vendor_ko requires both init_module AND cleanup_module
            # in the host so it can re-stamp both symbols' kCFI hashes
            # onto kh_entry / kh_exit.
            has_exit=$(echo "$nm_out" | grep -cE '^[0-9a-f]+ T cleanup_module$')
            ksym=$(echo "$nm_out" | grep -cE ' __ksymtab_[A-Za-z]')
            if [ "$has_init" -eq 1 ] && [ "$has_exit" -eq 1 ] && [ "$ksym" -eq 0 ] && [ "$sz" -lt "$best_size" ]; then
                best="$m"
                best_size="$sz"
            fi
        done < "$tmpd/mods.txt"
        if [ -n "$best" ]; then
            echo "$tmpd/$best"
            return 0
        fi
    done
    return 1
}

TMPD="/tmp/kh_graft_dev_$SERIAL"
rm -rf "$TMPD"; mkdir -p "$TMPD"
HOST=$(pick_host_ko "$TMPD")
if [ -z "$HOST" ]; then
    echo "ERROR: no suitable host .ko found in /vendor/lib/modules or /system/lib/modules" >&2
    exit 1
fi
printf "  host: %s\n" "$(basename "$HOST")"

"$GRAFT" --host "$HOST" --payload "$PAYLOAD" --out "$TMPD/grafted.ko" \
         --kallsyms-addr "0x$KADDR" >/tmp/kh_graft_tool.log 2>&1 || {
    echo "ERROR: graft_vendor_ko failed:" >&2
    tail -20 /tmp/kh_graft_tool.log >&2
    exit 1
}

# Push grafted artifact + loader. Push to /data/local/tmp (world-writable).
$ADB push "$TMPD/grafted.ko" /data/local/tmp/grafted.ko >/dev/null 2>&1
$ADB push "$LOADER"          /data/local/tmp/kmod_loader >/dev/null 2>&1
dsu "chmod +x /data/local/tmp/kmod_loader; dmesg -c >/dev/null 2>&1; true" >/dev/null 2>&1 || true

# Unload any stale grafted/consumer modules from a previous run. Names
# inside the grafted .ko inherit the host module's identity, so use the
# host's basename without .ko for rmmod.
HOST_NAME=$(basename "$HOST" .ko)
stale_cmd=""
for (( _i=${#CONSUMER_ARR[@]}-1; _i>=0; _i-- )); do
    stale_cmd+="rmmod ${CONSUMER_ARR[$_i]} 2>/dev/null; "
done
stale_cmd+="rmmod $HOST_NAME 2>/dev/null; "
stale_cmd+="rmmod kernelhook 2>/dev/null; true"
dsu "$stale_cmd" >/dev/null 2>&1 || true

# Live kmsg capture for post-mortem if grafted insmod panics the kernel.
# /dev/kmsg dumps the entire ring buffer from the start, so the file ends
# up with hours of unrelated boot/runtime messages — including any
# historical Oops/panic that has nothing to do with our test.  Inject a
# sentinel marker so the post-test scan skips pre-existing frames.
# Sentinel beats `stat -f%z`-based offset because `cat /dev/kmsg` block-
# buffers stdout to a file (no flush guarantee in 1s); a stat read can
# return 0 bytes while a multi-KB pre-test history block sits queued in
# the cat process buffer about to be flushed.
LIVE_KMSG="/tmp/kh_graft_dmesg_${SERIAL}.log"
rm -f "$LIVE_KMSG"
$ADB shell "su -c 'cat /dev/kmsg'" > "$LIVE_KMSG" 2>&1 &
KMSG_PID=$!
sleep 1
KMSG_BOUNDARY="KH_GRAFT_BOUNDARY_$$"
dsu "echo '$KMSG_BOUNDARY' > /dev/kmsg" >/dev/null 2>&1 || true

# Stage 1: insmod grafted (carries kernelhook payload). The grafted .ko
# was given the host module's kCFI initcall hash, so the kernel accepts
# it as a "vendor module" rather than rejecting the typeid.
INSMOD_OUT=$(perl -e 'alarm 60; exec @ARGV' \
    $ADB shell "su -c 'insmod /data/local/tmp/grafted.ko 2>&1; echo rc=\$?'" 2>&1 | tr -d '\r')
sleep 2

# Always grab whatever dmesg has after stage 1 — even if rc=0, this gives
# the user evidence of "kernelhook: loaded successfully".
S1_DMESG=$(dsu "dmesg | tail -60" 2>/dev/null)

if ! echo "$INSMOD_OUT" | grep -q '^rc=0$'; then
    printf "  ${KH_RED}FAIL${KH_RESET} grafted insmod failed\n"
    echo "$INSMOD_OUT" | sed 's/^/    /'
    echo "$S1_DMESG"  | sed 's/^/    dmesg: /'
    kill "$KMSG_PID" 2>/dev/null || true; wait "$KMSG_PID" 2>/dev/null || true
    if [ -s "$LIVE_KMSG" ] && grep -qE "BUG:|Unable to handle|Oops|Kernel panic|Call trace:" "$LIVE_KMSG"; then
        printf "\n  ${KH_YELLOW}Kernel panic in live kmsg:${KH_RESET}\n"
        awk '/BUG:|Unable to handle|Oops|Kernel panic|Call trace:/{p=1} p{print; if(++n>80) exit}' "$LIVE_KMSG" \
            | sed 's/^/    /'
    fi
    exit 1
fi
if ! echo "$S1_DMESG" | grep -q "kernelhook: loaded successfully"; then
    printf "  ${KH_RED}FAIL${KH_RESET} grafted .ko inserted but no 'kernelhook: loaded successfully' in dmesg\n"
    echo "$S1_DMESG" | sed 's/^/    /'
    kill "$KMSG_PID" 2>/dev/null || true; wait "$KMSG_PID" 2>/dev/null || true
    exit 1
fi
printf "  ${KH_GREEN}OK${KH_RESET}   grafted kernelhook loaded\n"

# Stage 2: per-consumer load + marker check. Same race-prevention pattern
# as test_avd_kmod.sh — run loader and grep dmesg in the same su shell so
# the marker line is captured before the next openat() spam evicts it.
ALL_PASS=1
for c in "${CONSUMER_ARR[@]}"; do
    $ADB push "$ROOT/examples/$c/$c.ko" "/data/local/tmp/$c.ko" >/dev/null 2>&1
    MARKER="$(consumer_marker "$c")"
    _esc_marker=$(printf '%s' "$MARKER" | sed -e 's/[\\"`$]/\\&/g' -e "s/'/'\\\\''/g")
    LOAD_OUTPUT=$(perl -e 'alarm 60; exec @ARGV' \
        $ADB shell "su -c '/data/local/tmp/kmod_loader /data/local/tmp/$c.ko kallsyms_addr=0x${KADDR} 2>&1; echo __KH_MARK_GREP__; dmesg 2>/dev/null | grep -F \"$_esc_marker\" | tail -1'" 2>&1 | tr -d '\r') || true
    _race_marker_line=$(echo "$LOAD_OUTPUT" | awk '/__KH_MARK_GREP__/{found=1; next} found' | tail -1)
    LOADER_STATUS=$(echo "$LOAD_OUTPUT" | awk '/__KH_MARK_GREP__/{exit} {print}')
    if ! echo "$LOADER_STATUS" | grep -qi "loaded"; then
        printf "  ${KH_RED}FAIL${KH_RESET} consumer %s load failed\n" "$c"
        echo "$LOADER_STATUS" | sed 's/^/    /'
        ALL_PASS=0
        continue
    fi
    if [ -n "$_race_marker_line" ]; then
        printf "  ${KH_GREEN}OK${KH_RESET}   %s (marker '%s')\n" "$c" "$MARKER"
        continue
    fi
    sleep 2
    if dsu "dmesg" 2>/dev/null | grep -Fq "$MARKER"; then
        printf "  ${KH_GREEN}OK${KH_RESET}   %s (marker '%s', late)\n" "$c" "$MARKER"
    elif dsu "dmesg" 2>/dev/null | grep -Eq "\\[KH/I\\] $c:"; then
        printf "  ${KH_GREEN}OK${KH_RESET}   %s (KH/I marker fallback)\n" "$c"
    else
        printf "  ${KH_RED}FAIL${KH_RESET} %s: marker not seen\n" "$c"
        ALL_PASS=0
    fi

    # hello_hook fire check — confirm the before-callback actually runs on
    # a triggered open(2), not just that the install pr_info hit dmesg.
    if [ "$c" = "hello_hook" ]; then
        dsu "cat /proc/version >/dev/null 2>&1; ls /data/local/tmp >/dev/null 2>&1; true" >/dev/null 2>&1 || true
        sleep 1
        FIRE_COUNT=$(dsu "dmesg" 2>/dev/null | grep -c "hello_hook: open called" || true)
        if [ "${FIRE_COUNT:-0}" -lt 1 ]; then
            printf "  ${KH_RED}FAIL${KH_RESET} hello_hook: before-callback never fired\n"
            ALL_PASS=0
        else
            printf "  ${KH_GREEN}OK${KH_RESET}   hello_hook fire: callback invoked %d time(s)\n" "$FIRE_COUNT"
        fi
    fi

    dsu "rmmod $c 2>/dev/null; true" >/dev/null 2>&1 || true
done

# Final cleanup: tear down hook before unloading grafted host (the host's
# rmmod path runs the kernelhook payload's exit, which uninstalls all hooks).
dsu "rmmod $HOST_NAME 2>/dev/null; true" >/dev/null 2>&1 || true

sleep 1
kill "$KMSG_PID" 2>/dev/null || true
wait "$KMSG_PID" 2>/dev/null || true

if [ -s "$LIVE_KMSG" ]; then
    # Only inspect log lines after the sentinel — kmsg buffer may
    # contain hours of unrelated history (vendor driver Oopses, etc.)
    # that would otherwise produce false-positive panic dumps.
    KMSG_TAIL=$(awk -v marker="$KMSG_BOUNDARY" \
        '$0 ~ marker {p=1; next} p' "$LIVE_KMSG" 2>/dev/null)
    if echo "$KMSG_TAIL" | grep -qE "BUG:|Unable to handle|Oops|Kernel panic|Call trace:"; then
        printf "\n  ${KH_YELLOW}Kernel panic during graft test:${KH_RESET}\n"
        echo "$KMSG_TAIL" | awk '/BUG:|Unable to handle|Oops|Kernel panic|Call trace:/{p=1} p{print; if(++n>80) exit}' \
            | sed 's/^/    /'
    fi
fi

if [ "$ALL_PASS" -eq 1 ]; then
    printf "\n${KH_BOLD}${KH_GREEN}[PASS] %s graft mode${KH_RESET}\n" "$SERIAL"
    exit 0
fi
printf "\n${KH_BOLD}${KH_RED}[FAIL] %s graft mode${KH_RESET}\n" "$SERIAL"
exit 1
