# Follow-up work tracker

Each file in this directory is one open follow-up item: a single
focused piece of deferred work, with enough context for any
contributor to pick up and execute without re-investigating.

## Open follow-ups (as of 2026-04-10)

| Priority | Doc | Topic | Estimated effort |
|---|---|---|---|
| **High** | [`2026-04-10-ring3-shim-padding-bump.md`](2026-04-10-ring3-shim-padding-bump.md) | One-line bump in `kmod/shim/shim.h:305` (`THIS_MODULE_SIZE 0x440 → 0x800`) to unblock Ring 3 (`exporter.ko`/`importer.ko`) on GKI 6.6+ | 15 min |
| **High** | [`2026-04-10-ddk-and-local-build-scripts.md`](2026-04-10-ddk-and-local-build-scripts.md) | **Architectural directive**: migrate kbuild path to AOSP DDK (Bazel) and extract `.github/workflows/kbuild.yml`'s logic into `scripts/build/` so contributors can run it locally. **Modifies the execution path of Plan 5.** | 4-5 days (phased) |
| Medium | [`2026-04-10-plan5-kh-test-kbuild.md`](2026-04-10-plan5-kh-test-kbuild.md) | Make `tests/kmod/kh_test.ko` build under kbuild — mechanical replication of Plan 4 fixes inside `tests/kmod/`. **Final step now writes `BUILD.bazel`** instead of `Kbuild` per the DDK directive. | 1-2 days |
| Medium | [`2026-04-10-vendor-ko-walker-this-module-size.md`](2026-04-10-vendor-ko-walker-this-module-size.md) | Tasks 3 + 4 from sh_size plan: preset-free `VAL_THIS_MODULE_SIZE` resolver via vendor `.ko` ELF walker | 1-2 hours |
| Medium | [`2026-04-10-kh-crc-binary-untracked.md`](2026-04-10-kh-crc-binary-untracked.md) | Repo hygiene: ghost aarch64 `kh_crc` binary materializes in fresh worktrees despite `.gitignore` | 30 min – 1 hour |
| Medium | [`2026-04-10-plan6-fake-linux-header-tree.md`](2026-04-10-plan6-fake-linux-header-tree.md) | Architectural cleanup of the **freestanding** lane: replace per-file `#ifdef KMOD_FREESTANDING` dual-path includes with a single fake `<linux/...>` header tree. Independent from the DDK directive (which only affects the kbuild lane). | Multi-day |
| Low | [`2026-04-10-extract-avd-crcs-6.12-mismatch.md`](2026-04-10-extract-avd-crcs-6.12-mismatch.md) | `extract_avd_crcs.py` produces a `module_layout` CRC that GKI 6.12 rejects at load (taint-only today, becomes blocking with `MODULE_SIG_FORCE`) | Investigation required |

## Suggested execution order

1. **Ring 3 padding bump** — fastest, highest user-visible impact (closes the last red mark on the GKI 6.6+ verification matrix from PR #1). Independent from everything else.
2. **kh_crc ghost binary** — small repo-hygiene win that unblocks fresh worktrees on darwin hosts. Independent.
3. **DDK directive Phase 1** (script extraction, no DDK yet) — pure refactor of `.github/workflows/kbuild.yml` into `scripts/build/`. Output bytes identical to current CI.
4. **Plan 5 (kh_test kbuild)** — `tests/kmod/*.c` mechanical cleanups now run against the new local-runnable script infrastructure. Removes the `!cancelled()` workaround in CI.
5. **DDK directive Phase 2** — introduce DDK Bazel rules, write `BUILD.bazel` files for `kernelhook`, `kbuild_hello`, and (now Plan 5 has landed) `kh_test`.
6. **DDK directive Phase 3** — switch CI to call DDK paths instead of legacy make paths. Workflow shrinks to ~30 lines.
7. **DDK directive Phase 4** — local-runnable docs in `docs/build-modes.md`.
8. **Vendor `.ko` walker (Tasks 3+4 from sh_size plan)** — removes preset-database maintenance burden for future GKI versions. Independent from DDK / Plan 5 / Plan 6.
9. **Plan 6 (fake `<linux/...>` tree)** — long-term refactor of the freestanding lane. Independent from DDK (which is the kbuild lane). Do once Plan 5 lands so the dual-path inventory is stable.
10. **6.12 CRC mismatch** — investigation-heavy; defer until someone needs `MODULE_SIG_FORCE` clean operation on 6.12.

## Cross-references

- PR #1 (sh_size fix, merged in `d96953d`):
  - Investigation: `docs/superpowers/investigations/2026-04-09-gki-6.6-6.12-load-failure.md`
  - Spec: `docs/superpowers/specs/2026-04-09-gki-6.6-sh_size-fix-design.md`
  - Plan: `docs/superpowers/plans/2026-04-09-gki-6.6-sh_size-fix.md`
  - Handoff: `docs/superpowers/handoffs/2026-04-10-gki-6.6-sh_size-fix-handoff.md`
- PR #2 (`feat/kbuild-interop`, Plan 3 + Plan 4, merged in `a25aa4b`)
- Reference commits for both PRs are inlined in each follow-up doc.
