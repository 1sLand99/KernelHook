#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Build out-of-tree GKI modules locally using the same DDK containers as CI.
#
# Requirements:
#   - Docker (or Podman aliased as docker)
#   - KernelHook source tree (this repository)
#
# Usage:
#   bash scripts/build/build_with_docker.sh android14-6.1
#   bash scripts/build/build_with_docker.sh android15-6.6 kh_test
#   bash scripts/build/build_with_docker.sh android16-6.12 kernelhook kbuild_hello kh_test
#
# Arguments:
#   $1        GKI branch (e.g., android14-6.1).  Required.
#   $2...     Module targets: kh_test, kernelhook, kbuild_hello.
#             Default: kh_test
#
# Output:
#   Artifact .ko files are copied to build/output/<branch>/ relative to
#   the repository root.
#
# Environment:
#   DDK_IMAGE_BASE  DDK container registry prefix
#                   (default: ghcr.io/ylarod/ddk-min)
#   DDK_IMAGE_TAG   DDK image tag suffix (default: 20260313)
#   DOCKER          Docker executable (default: docker)

set -euo pipefail

BRANCH="${1:?usage: build_with_docker.sh <branch> [module...]}"
shift

MODULES=("${@:-kh_test}")

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DDK_IMAGE_BASE="${DDK_IMAGE_BASE:-ghcr.io/ylarod/ddk-min}"
DDK_IMAGE_TAG="${DDK_IMAGE_TAG:-20260313}"
DOCKER="${DOCKER:-docker}"
IMAGE="${DDK_IMAGE_BASE}:${BRANCH}-${DDK_IMAGE_TAG}"

OUT_DIR="${ROOT}/build/output/${BRANCH}"
mkdir -p "$OUT_DIR"

# ---------- Helpers ----------

RED='\033[31m'; GREEN='\033[32m'; BOLD='\033[1m'; RESET='\033[0m'
ok()   { printf "${GREEN}ok${RESET}   %s\n" "$*"; }
bold() { printf "${BOLD}==> %s${RESET}\n" "$*"; }
fail() { printf "${RED}FAIL${RESET} %s\n" "$*" >&2; exit 1; }

# ---------- Preflight ----------

if ! command -v "$DOCKER" >/dev/null 2>&1; then
    fail "Docker not found. Install Docker Desktop or set DOCKER= to your container runtime."
fi

bold "Pulling DDK image (one-time download, ~500MB)"
"$DOCKER" pull "$IMAGE" || fail "Could not pull $IMAGE — check network and image name"

# ---------- Build in container ----------

KVER="${BRANCH##*-}"

build_module() {
    local target="$1"
    local m_path m_name

    case "$target" in
        kh_test)
            m_path="tests/kmod"
            m_name="kh_test.ko"
            ;;
        kernelhook)
            m_path="kmod"
            m_name="kernelhook.ko"
            ;;
        kbuild_hello)
            m_path="examples/kbuild_hello"
            m_name="kbuild_hello.ko"
            ;;
        *)
            fail "Unknown module target: $target (valid: kh_test, kernelhook, kbuild_hello)"
            ;;
    esac

    bold "Building $m_name from $m_path (branch=$BRANCH)"

    # Find kernel build dir inside the container.
    # DDK containers expose Module.symvers at a known path; discover it with
    # a short-lived find to avoid hardcoding per-image paths.
    local find_cmd='for d in /kernel /android-kernel /kernel-build /GKI_KERNEL_OUT; do
        [ -f "$d/Module.symvers" ] && echo "$d" && exit 0; done
        FOUND=$(find / -name "Module.symvers" -maxdepth 6 2>/dev/null | head -1)
        [ -n "$FOUND" ] && dirname "$FOUND" && exit 0
        echo "ERROR: Module.symvers not found" >&2; exit 1'

    local extra_flags=()
    if [ "$target" = "kbuild_hello" ]; then
        # kbuild_hello needs kernelhook's Module.symvers built first.
        if [ ! -f "$ROOT/kmod/Module.symvers" ]; then
            fail "kbuild_hello requires kernelhook.ko to be built first.
Run: bash scripts/build/build_with_docker.sh $BRANCH kernelhook"
        fi
        extra_flags+=(
            -e "KBUILD_EXTRA_SYMBOLS=/src/kmod/Module.symvers"
            -e "KERNELHOOK=/src"
        )
    fi

    "$DOCKER" run --rm \
        -v "${ROOT}:/src" \
        "${extra_flags[@]}" \
        "$IMAGE" \
        bash -c "
            set -euo pipefail
            KDIR=\$(${find_cmd})
            echo \"Using kernel build: \$KDIR\"
            make -C \"\$KDIR\" M=\"/src/${m_path}\" \
                 ARCH=arm64 LLVM=1 KBUILD_MODPOST_WARN=1 \
                 modules -j\$(nproc)
        "

    # Verify and copy
    local ko="${ROOT}/${m_path}/${m_name}"
    [[ -s "$ko" ]] || fail "Build succeeded but $ko is missing or empty"
    cp "$ko" "$OUT_DIR/${m_name}"
    ok "${m_name} -> build/output/${BRANCH}/${m_name}"
}

for mod in "${MODULES[@]}"; do
    build_module "$mod"
done

# ---------- Summary ----------

bold "Build complete — artifacts in build/output/${BRANCH}/"
ls -lh "$OUT_DIR/"
