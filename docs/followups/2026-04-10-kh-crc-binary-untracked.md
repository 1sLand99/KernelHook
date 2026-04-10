# Repo hygiene: `tools/kh_crc/kh_crc` is gitignored but ghost-binaries linger

**Status:** Open. Created 2026-04-10. Discovered while running PR #1's
AVD matrix in a fresh `git worktree add` of `main`. Pure dev-experience
issue; no functional impact on shipped artifacts.

## Symptom

In a freshly created worktree of `main`, running
`./scripts/test_avd_kmod.sh Pixel_3X` (which builds Ring 3
`exporter.ko`/`importer.ko` via `kmod/mk/kmod.mk`) fails with:

```
tools/kh_crc/kh_crc: cannot execute binary file
make[1]: *** [.../include/kernelhook/kh_symvers.h] Error 126
make: *** [exporter.ko] Error 2
```

Inspection:

```
$ file tools/kh_crc/kh_crc
tools/kh_crc/kh_crc: ELF 64-bit LSB pie executable, ARM aarch64,
                     version 1 (SYSV), dynamically linked,
                     interpreter /system/bin/linker64, not stripped
```

It's an **Android aarch64 binary** sitting at the path where the
build expects a **darwin native** host binary (used to generate
`kh_symvers.h` at build time).

## What's actually going on

`tools/kh_crc/.gitignore:1` contains `kh_crc`, and
`git ls-files tools/kh_crc/kh_crc` returns "no such file" — the
binary is **not tracked by git**. Yet a fresh worktree of `main`
materializes a stale aarch64 binary at that path. Where does it come
from?

Most likely cause: an earlier commit (now reverted/cleaned) committed
the binary into the index, then a later commit added it to
`.gitignore` without `git rm --cached` to remove the index entry. The
file is now an "orphan" sitting in the repo's pack history but
invisible to `git ls-files`. `git checkout`/`git worktree add`
reproduces it from history during checkout but `git status` doesn't
show it as tracked.

## Workaround used during PR #1's verification

```bash
cd tools/kh_crc
rm kh_crc
make           # darwin native rebuild
file kh_crc    # Mach-O 64-bit executable arm64
./tests/run_tests.sh   # Contract 4: 3/3 PASS
```

The rebuilt darwin-native binary then satisfies the export_link_test
build path. **Do NOT commit it** — it would break Linux CI and any
non-darwin host.

## Suggested fix

Two options, neither risky:

### Option A — purge the orphan from git history

```bash
git rm --cached tools/kh_crc/kh_crc 2>/dev/null || true
# Then verify .gitignore still has the entry
grep -F kh_crc tools/kh_crc/.gitignore
git commit -m "tools(kh_crc): purge ghost binary from index, was already gitignored"
```

If `git rm --cached` reports "did not match any file" then the orphan
isn't actually in any reachable commit's tree — in which case the
ghost might be from a `git checkout` quirk and the next worktree
creation may not reproduce it. Test by creating a fresh worktree from
the commit returned by `git log -1 --pretty=format:%H tools/kh_crc/kh_crc`
(if any) and verifying.

If the orphan is real, this commit removes it from future checkouts.

### Option B — auto-build on first use

Add a Make rule (or shell wrapper) so any consumer of `tools/kh_crc/kh_crc`
that finds the file missing or wrong-arch builds it on the fly. The
existing `tools/kh_crc/Makefile` already has `all: kh_crc`; the
consumers (`kmod/mk/kmod.mk`, etc.) just need to invoke it before use.
Sketch:

```make
# In kmod/mk/kmod.mk, before any rule that invokes kh_crc:
$(KH_ROOT)/tools/kh_crc/kh_crc:
	$(MAKE) -C $(KH_ROOT)/tools/kh_crc

# Then have rules depend on it:
$(KH_SYMVERS_H): $(KH_ROOT)/tools/kh_crc/kh_crc $(MANIFEST)
	$(KH_ROOT)/tools/kh_crc/kh_crc --mode=header --manifest=$(MANIFEST) --output=$@
```

Combined with Option A this fully decouples consumers from the host
arch question. Recommended.

## Interaction with Plan 5

Plan 5 (`docs/followups/2026-04-10-plan5-kh-test-kbuild.md`) touches
`tests/kmod/` build paths but not `tools/kh_crc/`. This followup is
independent.

## Risks

| Risk | Mitigation |
|---|---|
| Option A removes a binary someone is depending on | Verify with `git log --diff-filter=A -- tools/kh_crc/kh_crc` whether the file was ever intentionally committed and by whom. If unintentional, safe to purge. |
| Option B's auto-build adds a make recursion to every Ring 3 build | Cheap (kh_crc is ~200 LoC of C, builds in <1s); makes builds reproducible across machines. |

## Estimated effort

30 min for Option A + smoke test on a fresh worktree.
1 hour for Option B + Option A combined.

## Reference

- `tools/kh_crc/.gitignore` — already lists `kh_crc`
- `tools/kh_crc/Makefile` — has the build rule
- Contract 4 — kh_crc CRC bytes must not change. Verified during PR #1
  by `./tests/run_tests.sh` after rebuild: 3/3 PASS.
