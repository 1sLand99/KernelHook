# SPDX-License-Identifier: GPL-2.0-or-later
# bazel/ddk.bzl — KernelHook DDK build rules.
#
# Provides ddk_module() and ddk_headers() with the same call-site interface
# as AOSP's kleaf rules.  Implementation uses genrule() + make, delegating
# to the kernel build system in the DDK container.  No AOSP workspace needed.
#
# Migration path to upstream kleaf: when moving to a full AOSP kleaf setup,
# replace the load() in each BUILD.bazel from
#   load("//bazel:ddk.bzl", "ddk_module", "ddk_headers")
# to
#   load("@kleaf//build/kernel/kleaf:kernel.bzl", "ddk_module", "ddk_headers")
# No other changes to the BUILD.bazel files are required.

def ddk_headers(
        name,
        hdrs = [],
        includes = [],
        linux_includes = [],
        visibility = None,
        **kwargs):
    """Export a set of kernel headers for downstream ddk_module() targets.

    API-compatible with AOSP's ddk_headers().

    Args:
        name:           Target name.
        hdrs:           Header files to export.
        includes:       Include directories (for documentation; compilation
                        flags are set via ccflags-y in the module Kbuild).
        linux_includes: Extra linux/ include paths (API compat, not used).
        visibility:     Bazel visibility.
    """
    native.filegroup(
        name = name,
        srcs = hdrs,
        visibility = visibility or ["//visibility:public"],
    )

def ddk_module(
        name,
        srcs = [],
        hdrs = [],
        out = None,
        kernel_build = None,
        deps = [],
        includes = [],
        copts = [],
        visibility = None,
        **kwargs):
    """Build an out-of-tree GKI kernel module.

    API-compatible with AOSP's ddk_module().  Invokes:
      make -C $KDIR M=<pkg-srcdir> ARCH=arm64 LLVM=1 modules

    KDIR is read from bazel/kernel_build/kdir.txt, written by
    scripts/build/setup_bazel_ddk.sh.

    Args:
        name:         Target name, also the module name.
        srcs:         Source (.c, .S) and private header files.
        hdrs:         Exported header targets (filegroups / ddk_headers).
        out:          Output .ko name (defaults to <name>.ko).
        kernel_build: Kernel build label (API compat with kleaf; actual
                      KDIR comes from kdir.txt at build time).
        deps:         Upstream ddk_module/ddk_headers targets.
        includes:     Include directories (API compat; set via Kbuild).
        copts:        Extra compiler flags (API compat; set via Kbuild).
        visibility:   Bazel visibility.
    """
    ko_name = out or (name + ".ko")

    # Anchor source file: used to discover the package source directory.
    # Pick the first .c file in srcs; fall back to any file.
    anchor = None
    for s in srcs:
        if s.endswith(".c") or s.endswith(".S"):
            anchor = s
            break
    if anchor == None and srcs:
        anchor = srcs[0]

    # If no anchor, we can't build — emit a failing rule.
    if anchor == None:
        native.genrule(
            name = name,
            srcs = [],
            outs = [ko_name],
            cmd = "echo 'ERROR: ddk_module({}) has no srcs'; exit 1".format(name),
            visibility = visibility or ["//visibility:public"],
        )
        return

    # Collect all source inputs for the genrule.
    all_inputs = list(srcs) + ["//bazel/kernel_build:kdir_file"]
    for d in deps:
        all_inputs.append(d)

    native.genrule(
        name = name,
        srcs = all_inputs,
        outs = [ko_name],
        message = "DDK ddk_module: " + ko_name,
        cmd = """
set -euo pipefail

# ---- Locate KDIR ----
KDIR=$$(cat $(location //bazel/kernel_build:kdir_file))
if [ ! -f "$$KDIR/Module.symvers" ]; then
    echo "ERROR: KDIR $$KDIR has no Module.symvers"
    echo "Run: bash scripts/build/setup_bazel_ddk.sh <kdir>"
    exit 1
fi

# ---- Locate the package source directory ----
# $(execpath <anchor>) gives the absolute execroot path to the anchor file.
# The package dir is its dirname.
ANCHOR="$(execpath {anchor})"
PKG_DIR="$$(dirname "$$ANCHOR")"

# ---- Build via make ----
make -C "$$KDIR" \
     M="$$PKG_DIR" \
     ARCH=arm64 LLVM=1 \
     KBUILD_MODPOST_WARN=1 \
     modules -j$$(nproc)

# ---- Copy output .ko ----
KO="$$PKG_DIR/{ko_name}"
if [ ! -f "$$KO" ]; then
    echo "ERROR: {ko_name} not found in $$PKG_DIR after make"
    ls "$$PKG_DIR"/*.ko 2>/dev/null || true
    exit 1
fi
cp "$$KO" "$@"
echo "==> Built $@ from $$KDIR (via ddk_module)"
""".format(anchor = anchor, ko_name = ko_name),
        visibility = visibility or ["//visibility:public"],
    )
