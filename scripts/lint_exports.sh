#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Fail if any legacy bare (non-kh_) public API symbol appears in scope.
# Wired into scripts/test.sh sdk-consumer so regressions surface in CI.

set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Legacy patterns — every one MUST NOT appear as an independent word in scope.
# \b is word-boundary; _ is a word character so e.g. `\bhook_install\b`
# matches standalone `hook_install` but not `kh_hook_install`.
PATTERNS=(
  # bare hook_* functions
  '\bhook_install\b' '\bhook_uninstall\b' '\bhook_prepare\b'
  '\bhook_wrap\b' '\bhook_unwrap\b' '\bhook_unwrap_remove\b'

  # hook_chain_* functions
  '\bhook_chain_add\b' '\bhook_chain_remove\b' '\bhook_chain_setup_transit\b'

  # hook_mem_* (entire family — prefix replaces to kh_mem_)
  '\bhook_mem_[a-z0-9_]+\b'

  # hook_ typedefs
  '\bhook_chain_rox_t\b' '\bhook_chain_rw_t\b'
  '\bhook_fargs[0-9]+_t\b' '\bhook_local_t\b' '\bhook_err_t\b' '\bhook_t\b'
  '\bhook_chain[0-9]+_callback\b' '\bhook_chain_item_t\b'

  # hook_wrap0..12 / fp_hook_wrap0..12 numbered variants
  '\bhook_wrap[0-9]+\b' '\bfp_hook_wrap[0-9]+\b'

  # fp_hook_* family
  '\bfp_hook\b' '\bfp_unhook\b' '\bfp_hook_wrap\b' '\bfp_hook_unwrap\b'
  '\bfp_hook_chain_setup_transit\b' '\bfp_hook_t\b'
  '\bfp_hook_chain_rox_t\b' '\bfp_hook_chain_rw_t\b'

  # platform_* (entire family)
  '\bplatform_alloc_rox\b' '\bplatform_alloc_rw\b' '\bplatform_free\b'
  '\bplatform_page_size\b' '\bplatform_set_rw\b' '\bplatform_set_ro\b'
  '\bplatform_set_rx\b' '\bplatform_write_code\b' '\bplatform_flush_icache\b'

  # sync_*
  '\bsync_read_lock\b' '\bsync_read_unlock\b'
  '\bsync_write_lock\b' '\bsync_write_unlock\b' '\bsync_init\b'

  # hmem_user_*
  '\bhmem_user_init\b' '\bhmem_user_cleanup\b'

  # remote_hook_*
  '\bremote_hook_install\b' '\bremote_hook_alloc\b' '\bremote_hook_attach\b'
  '\bremote_hook_detach\b' '\bremote_hook_handle_t\b'

  # bare "unhook" (stand-alone syscall hook removal)
  '\bunhook\b'
)

SCOPE=(src include kmod tests examples)

FAIL=0
for pat in "${PATTERNS[@]}"; do
  if hits=$(grep -rnE --include='*.c' --include='*.h' --include='*.manifest' "$pat" "${SCOPE[@]}" 2>/dev/null); then
    if [ -n "$hits" ]; then
      printf '\033[31mFAIL\033[0m legacy symbol matched: %s\n' "$pat" >&2
      printf '%s\n' "$hits" >&2
      FAIL=$((FAIL+1))
    fi
  fi
done

if [ $FAIL -gt 0 ]; then
  printf '\n\033[31m=== lint_exports: %d legacy pattern(s) found ===\033[0m\n' "$FAIL" >&2
  exit 1
fi

printf '\033[32mlint_exports: OK (zero legacy patterns in %s)\033[0m\n' "${SCOPE[*]}"
exit 0
