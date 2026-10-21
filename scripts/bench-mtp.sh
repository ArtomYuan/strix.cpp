#!/usr/bin/env bash

# Canonical MTP benchmark for strix.cpp.
# Fixed 2026-10-21: depth 6, single stream, 128 tokens, /completion.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <model.gguf> [output.txt] [port] [depth]" >&2
    echo "env: STRIX_SERVER (default ./bin/strix-server)" >&2
    exit 2
fi

MODEL=$1
OUT=${2:-"$(basename "${MODEL%.gguf}").mtp.txt"}
PORT=${3:-8098}
DEPTH=${4:-6}
SERVER=${STRIX_SERVER:-./bin/strix-server}
LOG="/tmp/strix-mtp-${PORT}.log"

mkdir -p "$(dirname "$OUT")"

"$SERVER" \
    -m "$MODEL" \
    -c 32768 \
    -np 1 \
    --port "$PORT" \
    --host 127.0.0.1 \
    --reasoning off \
    --spec-type draft-mtp \
    --spec-draft-n-max "$DEPTH" > "$LOG" 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null || true' EXIT

for _ in $(seq 1 90); do
    if curl -sf "http://127.0.0.1:${PORT}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 2
done

RESP=$(curl -s "http://127.0.0.1:${PORT}/completion" \
    -H 'Content-Type: application/json' \
    -d '{"prompt":"Write a short story about a robot who learns to paint.","n_predict":128,"temperature":0}')

{
    echo "model=$MODEL"
    echo "port=$PORT"
    echo "depth=$DEPTH"
    grep -aE "eval time|draft acceptance" "$LOG" | tail -2 || true
    echo "response=$(printf '%s' "$RESP" | head -c 500)"
} > "$OUT"

echo "wrote $OUT"
