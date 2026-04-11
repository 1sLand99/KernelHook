#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Set up a kbuild environment for building out-of-tree GKI modules.
#
# NOTE: CI now uses DDK containers (ghcr.io/ylarod/ddk-min) which bundle
# the correct toolchain and kernel headers. This script is kept for local
# builds on a Linux host where you want to compile against kernel source.
#
# Installs host toolchain (if apt available), clones kernel source (if
# KERNEL_DIR is empty), runs defconfig + modules_prepare + vmlinux.
#
# Required env:
#   BRANCH       — GKI branch (e.g., android14-6.1)
#   KERNEL_DIR   — where to clone/find kernel source
#   KERNEL_OUT   — kernel output directory (O=)
#
# Optional env:
#   SKIP_APT=1   — skip apt install (for local use on non-Debian hosts)

set -euo pipefail

: "${BRANCH:?BRANCH is required (e.g., android14-6.1)}"
: "${KERNEL_DIR:?KERNEL_DIR is required}"
: "${KERNEL_OUT:?KERNEL_OUT is required}"

# ---------- Host toolchain ----------

if [ "${SKIP_APT:-0}" != "1" ] && command -v apt-get >/dev/null 2>&1; then
    echo "==> Installing host toolchain"
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
        bc bison flex libssl-dev libelf-dev libdw-dev cpio kmod python3 ccache \
        clang lld llvm llvm-dev \
        gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
        rsync zstd xz-utils \
        dwarves

    # Kernel 5.10 needs Google's prebuilt clang (upstream clang rejects
    # global register variables in asm/stack_pointer.h).
    KVER="${BRANCH##*-}"
    KVER_MAJOR="${KVER%%.*}"
    KVER_MINOR="${KVER#*.}"
    if [ "$KVER_MAJOR" -eq 5 ] && [ "$KVER_MINOR" -le 10 ] 2>/dev/null; then
        CLANG_REV=r416183b
        CLANG_URL="https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/android12-release/clang-${CLANG_REV}.tar.gz"
        CLANG_DIR="$HOME/clang-${CLANG_REV}"
        if [ ! -d "$CLANG_DIR/bin" ]; then
            echo "==> Downloading Google prebuilt clang-${CLANG_REV} for kernel $KVER"
            mkdir -p "$CLANG_DIR"
            curl -sSL "$CLANG_URL" | tar xz -C "$CLANG_DIR"
        fi
        export PATH="$CLANG_DIR/bin:$PATH"
        if [ -n "${GITHUB_ENV:-}" ]; then
            echo "PATH=$CLANG_DIR/bin:$PATH" >> "$GITHUB_ENV"
        fi
    fi

    clang --version
    ld.lld --version
fi

# ---------- Kernel source ----------

if [ ! -d "$KERNEL_DIR/.git" ]; then
    echo "==> Cloning kernel/common branch $BRANCH"
    git clone --depth=1 --single-branch \
        --branch "$BRANCH" \
        https://android.googlesource.com/kernel/common "$KERNEL_DIR"
else
    echo "==> Kernel source already present at $KERNEL_DIR"
fi

# ---------- Check for cached build output ----------

if [ -f "$KERNEL_OUT/Module.symvers" ] && [ -f "$KERNEL_OUT/.config" ]; then
    echo "==> Cached build output found, skipping configure + vmlinux build"
    make -C "$KERNEL_DIR" O="$KERNEL_OUT" ARCH=arm64 LLVM=1 \
         modules_prepare -j"$(nproc)"
    echo "==> Kbuild environment ready (cached)"
    exit 0
fi

# ---------- Configure ----------

if [ -f "$KERNEL_DIR/arch/arm64/configs/gki_defconfig" ]; then
    CFG=gki_defconfig
else
    CFG=defconfig
fi
echo "==> Configuring with $CFG"
make -C "$KERNEL_DIR" O="$KERNEL_OUT" ARCH=arm64 LLVM=1 "$CFG"

# Kernel 6.12+ needs pahole >= 1.26 for BTF; Ubuntu 24.04 has 1.25.
# We only need Module.symvers for out-of-tree builds, not BTF.
KVER="${BRANCH##*-}"
KVER_MAJOR="${KVER%%.*}"
KVER_MINOR="${KVER#*.}"
if [ "$KVER_MAJOR" -ge 6 ] && [ "$KVER_MINOR" -ge 12 ] 2>/dev/null; then
    echo "==> Disabling BTF for kernel $KVER (pahole too old)"
    "$KERNEL_DIR/scripts/config" --file "$KERNEL_OUT/.config" --disable DEBUG_INFO_BTF
    make -C "$KERNEL_DIR" O="$KERNEL_OUT" ARCH=arm64 LLVM=1 olddefconfig
fi

# ---------- modules_prepare ----------

echo "==> modules_prepare"
make -C "$KERNEL_DIR" O="$KERNEL_OUT" ARCH=arm64 LLVM=1 \
     modules_prepare -j"$(nproc)"

# ---------- vmlinux (for Module.symvers) ----------

echo "==> Building vmlinux (for Module.symvers)"
make -C "$KERNEL_DIR" O="$KERNEL_OUT" ARCH=arm64 LLVM=1 \
     vmlinux -j"$(nproc)" || \
make -C "$KERNEL_DIR" O="$KERNEL_OUT" ARCH=arm64 LLVM=1 \
     Image -j"$(nproc)"

echo "==> Kbuild environment ready"
