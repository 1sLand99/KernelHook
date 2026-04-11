#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Verify a kbuild-produced kh_test.ko:
#   - file exists and is non-empty
#   - modinfo vermagic contains expected kernel version substring
#   - __versions section has CRCs (modpost ran)
#   - undefined ELF symbols are all in the kernel-exported allowlist
#
# Usage: verify_kmod.sh <path-to-kh_test.ko> <expected-kver-substring>

set -euo pipefail

KO="${1:?usage: verify_kmod.sh <kh_test.ko> <kver>}"
KVER="${2:?usage: verify_kmod.sh <kh_test.ko> <kver>}"

RED='\033[31m'; GREEN='\033[32m'; YELLOW='\033[33m'; RESET='\033[0m'
fail() { printf "${RED}FAIL${RESET} %s\n" "$*" >&2; exit 1; }
ok()   { printf "${GREEN}ok${RESET}   %s\n" "$*"; }
warn() { printf "${YELLOW}warn${RESET} %s\n" "$*"; }

[[ -s "$KO" ]] || fail "missing or empty: $KO"
ok "file present: $KO ($(stat -c%s "$KO" 2>/dev/null || stat -f%z "$KO") bytes)"

MODINFO="$(command -v modinfo || true)"
[[ -n "$MODINFO" ]] || fail "modinfo not found (apt install kmod)"

VERMAGIC="$(modinfo -F vermagic "$KO" || true)"
[[ -n "$VERMAGIC" ]] || fail "no vermagic in $KO"
ok "vermagic: $VERMAGIC"
[[ "$VERMAGIC" == *"$KVER"* ]] || fail "vermagic missing expected kver '$KVER'"

READELF="$(command -v llvm-readelf || command -v readelf || true)"
[[ -n "$READELF" ]] || fail "readelf not found"

# __versions section indicates modpost wrote CRC table
if $READELF -S "$KO" 2>/dev/null | grep -q '__versions'; then
    ok "__versions section present"
else
    warn "no __versions section (CONFIG_MODVERSIONS disabled?)"
fi

# Undefined symbol allowlist: anything exported by the kernel we actually need.
# Entries are regex alternatives matched against the 8th column of readelf -s.
ALLOW='^(_?printk|printk_ratelimit|memcpy|memset|memmove|memcmp|__memcpy|__memset|__memmove|'\
'module_layout|__this_module|init_module|cleanup_module|'\
'kallsyms_lookup_name|kallsyms_on_each_symbol|'\
'__stack_chk_fail|__stack_chk_guard|'\
'_GLOBAL_OFFSET_TABLE_|__gnu_mcount_nc|mcount|'\
'strlen|strnlen|strcmp|strncmp|strcpy|strncpy|strchr|strrchr|snprintf|scnprintf|vsnprintf|sprintf|'\
'kmalloc|kmalloc_trace|__kmalloc|kfree|krealloc|kzalloc|kvmalloc|kvfree|kvmalloc_node|vmalloc|vfree|'\
'__kmalloc_noprof|kmalloc_trace_noprof|vmalloc_noprof|'\
'mutex_lock|mutex_unlock|__mutex_init|spin_lock|spin_unlock|_raw_spin_lock|_raw_spin_unlock|'\
'_raw_spin_lock_irqsave|_raw_spin_unlock_irqrestore|'\
'msleep|schedule|wake_up_process|'\
'copy_from_user|copy_to_user|_copy_from_user|_copy_to_user|'\
'on_each_cpu|flush_icache_range|__flush_dcache_area|caches_clean_inval_pou|'\
'cpus_read_lock|cpus_read_unlock|stop_machine|'\
'set_memory_ro|set_memory_rw|set_memory_x|set_memory_nx|'\
'register_kprobe|unregister_kprobe|'\
'fortify_panic|__fortify_panic|__fortify_report|'\
'mem_alloc_profiling_key|'\
'__arm64_sys_.*|aarch64_insn_.*|'\
'preempt_count_add|preempt_count_sub|__preempt_count_add|__preempt_count_sub|'\
'__cfi_slowpath|__cfi_slowpath_diag|__ubsan_handle_cfi_check_fail_abort)$'

BAD=0
while IFS= read -r sym; do
    [[ -z "$sym" ]] && continue
    if ! [[ "$sym" =~ $ALLOW ]]; then
        printf "${RED}  unexpected undef:${RESET} %s\n" "$sym" >&2
        BAD=$((BAD + 1))
    fi
done < <($READELF -s "$KO" 2>/dev/null | awk '$7=="UND" && $8!="" {print $8}' | sort -u)

if [[ $BAD -gt 0 ]]; then
    fail "$BAD unexpected undefined symbol(s) — extend allowlist or fix code"
fi
ok "all undefined symbols in allowlist"

printf "${GREEN}PASS${RESET} %s vermagic='%s'\n" "$(basename "$KO")" "$VERMAGIC"
