#!/usr/bin/env bash
# One-click installer: clone, build, and install strix.cpp into ./strix.cpp.
# Override with INSTALL_DIR=..., STRIX_REPO=..., STRIX_REF=...
set -euo pipefail

REPO_URL="${STRIX_REPO:-https://github.com/ArtomYuan/strix.cpp.git}"
REF="${STRIX_REF:-master}"
INSTALL_DIR="${INSTALL_DIR:-$PWD/strix.cpp}"

# Already inside a checkout: just build/install it.
if [[ -f "$PWD/CMakeLists.txt" && -f "$PWD/scripts/install.sh" ]]; then
    exec "$PWD/scripts/install.sh"
fi

if [[ -e "$INSTALL_DIR" ]] && [[ -n "$(ls -A "$INSTALL_DIR" 2>/dev/null || true)" ]]; then
    echo "error: $INSTALL_DIR already exists and is not empty; set INSTALL_DIR to another path" >&2
    exit 1
fi

SRC_DIR="$INSTALL_DIR/src"
mkdir -p "$INSTALL_DIR"
git clone --depth 1 --branch "$REF" "$REPO_URL" "$SRC_DIR"

INSTALL_DIR="$INSTALL_DIR" "$SRC_DIR/scripts/install.sh"
