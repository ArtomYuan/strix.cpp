#!/usr/bin/env bash
# Build strix-server and install it into a directory chosen by the caller.
# Default: $PWD/strix.cpp when run from a parent directory, or the repo root
# when run from inside the checkout. Override with INSTALL_DIR=...
set -euo pipefail

SRC_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -n "${INSTALL_DIR:-}" ]]; then
    :
else
    INSTALL_DIR="$PWD/strix.cpp"
fi

if [[ -z "${ROCM_PATH:-}" ]]; then
    for candidate in /opt/rocm /opt/rocm/core-10.1; do
        if [[ -d "$candidate/lib/cmake" ]]; then
            ROCM_PATH="$candidate"
            break
        fi
    done
fi

if [[ -z "${ROCM_PATH:-}" ]]; then
    echo "error: ROCm not found; set ROCM_PATH" >&2
    exit 1
fi

BUILD_DIR="$SRC_DIR/build"
TARGETS="${AMDGPU_TARGETS:-gfx1151}"

echo "source:  $SRC_DIR"
echo "install: $INSTALL_DIR"
echo "ROCm:    $ROCM_PATH"
echo "targets: $TARGETS"

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DGGML_HIP=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DAMDGPU_TARGETS="$TARGETS" \
    -DCMAKE_PREFIX_PATH="$ROCM_PATH" \
    -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib' \
    -DLLAMA_CURL=OFF \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DGGML_OPENMP=OFF

cmake --build "$BUILD_DIR" -j "$(nproc)"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"

test -x "$INSTALL_DIR/bin/strix-server"
echo "installed: $INSTALL_DIR/bin/strix-server"
echo "run with: LD_LIBRARY_PATH=$INSTALL_DIR/lib $INSTALL_DIR/bin/strix-server -m <model.gguf> --port 8080"
