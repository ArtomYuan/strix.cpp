#!/usr/bin/env python3
"""Pair two llama-bench JSONL streams and print a ratio table.

Rows are keyed by (test, n_prompt, n_gen, n_depth); the compared value is
avg_ts (tokens per second). A ratio above 1.0 means the strix side is faster.

The point of the table is not "we beat upstream on our own format" — that is
expected. It is "the standard quantization paths did not regress", which is what
same-quant rows show.

Usage:
  compare.py strix.jsonl upstream.jsonl [--json]
Exit code: 0 when at least one row pairs up, 1 otherwise.
"""

import argparse
import json
import sys


def load(path):
    rows = {}
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            key = (
                row.get("test"),
                row.get("n_prompt"),
                row.get("n_gen"),
                row.get("n_depth", 0),
            )
            rows[key] = row
    return rows


def main():
    parser = argparse.ArgumentParser(description="Compare two llama-bench JSONL streams")
    parser.add_argument("strix")
    parser.add_argument("upstream")
    parser.add_argument("--json", action="store_true", help="print JSON instead of markdown")
    args = parser.parse_args()

    strix = load(args.strix)
    upstream = load(args.upstream)
    keys = sorted(set(strix) & set(upstream), key=lambda k: (str(k[0]), k[1] or 0, k[2] or 0))

    if not keys:
        print("no overlapping (test, n_prompt, n_gen) rows — did both sides run the same cases?",
              file=sys.stderr)
        return 1

    rows = []
    for key in keys:
        left = strix[key].get("avg_ts")
        right = upstream[key].get("avg_ts")
        if not left or not right:
            continue
        rows.append({
            "test": key[0],
            "n_prompt": key[1],
            "n_gen": key[2],
            "strix_ts": round(left, 2),
            "upstream_ts": round(right, 2),
            "ratio": round(left / right, 3),
        })

    if not rows:
        print("rows paired up but avg_ts was missing on one side", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0

    print("| test | n_prompt | n_gen | strix t/s | upstream t/s | ratio |")
    print("|:--|--:|--:|--:|--:|--:|")
    for row in rows:
        print(f"| {row['test']} | {row['n_prompt']} | {row['n_gen']} | "
              f"{row['strix_ts']} | {row['upstream_ts']} | {row['ratio']} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
