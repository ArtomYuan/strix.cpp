#!/usr/bin/env bash
# Run the same benchmark shapes through two llama-bench binaries and keep both
# JSONL streams. Nothing is interpreted here; compare.py does the pairing.
#
# usage: run.sh --model M.gguf --strix path/to/strix/llama-bench \
#               --upstream path/to/upstream/llama-bench [--out DIR]
#
# Both sides must be built for the same GPU, ROCm version and model file.
# Record clocks/temperature yourself: this script measures throughput only.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
cases="$here/cases.json"
model=""
strix=""
upstream=""
out="$root/build/parity"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --model)    model="$2";    shift 2 ;;
        --strix)    strix="$2";    shift 2 ;;
        --upstream) upstream="$2"; shift 2 ;;
        --out)      out="$2";      shift 2 ;;
        -h|--help)
            sed -n '2,10p' "$0"
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

if [[ -z "$model" || -z "$strix" || -z "$upstream" ]]; then
    echo "need --model, --strix and --upstream" >&2
    exit 2
fi
[[ -f "$model" ]]  || { echo "model not found: $model" >&2; exit 2; }
[[ -x "$strix" ]]  || { echo "not executable: $strix" >&2; exit 2; }
[[ -x "$upstream" ]] || { echo "not executable: $upstream" >&2; exit 2; }

mkdir -p "$out"
: > "$out/strix.jsonl"
: > "$out/upstream.jsonl"

# cases.json stays the single definition of the shapes.
shapes="$(python3 - "$cases" <<'PY'
import json, sys
for case in json.load(open(sys.argv[1]))["cases"]:
    print(case["id"], case["pp"], case["tg"], case["reps"])
PY
)"

echo "== sides: strix=$strix upstream=$upstream" >&2
echo "== model: $model" >&2

while read -r id pp tg reps; do
    [[ -n "$id" ]] || continue
    echo "-- $id (pp=$pp tg=$tg reps=$reps)" >&2
    "$strix"    -m "$model" -p "$pp" -n "$tg" -r "$reps" -o jsonl >> "$out/strix.jsonl"
    "$upstream" -m "$model" -p "$pp" -n "$tg" -r "$reps" -o jsonl >> "$out/upstream.jsonl"
done <<< "$shapes"

python3 "$here/compare.py" "$out/strix.jsonl" "$out/upstream.jsonl"
