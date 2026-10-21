#!/usr/bin/env bash

# Canonical HF model benchmark for strix.cpp.
# Fixed 2026-10-21; do not change these parameters without updating
# docs/bench/README.md and the strix benchmark skill.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <model.gguf> [output.json]" >&2
    echo "env: LLAMA_BENCH (default ./bin/llama-bench)" >&2
    exit 2
fi

MODEL=$1
OUT=${2:-"$(basename "${MODEL%.gguf}").bench.json"}
BENCH=${LLAMA_BENCH:-./bin/llama-bench}

mkdir -p "$(dirname "$OUT")"

"$BENCH" \
    -m "$MODEL" \
    -p 256,2048 \
    -n 256 \
    -r 5 \
    -fa 1 \
    -ngl 999 \
    -ub 512 \
    -o json > "$OUT"

echo "wrote $OUT"
