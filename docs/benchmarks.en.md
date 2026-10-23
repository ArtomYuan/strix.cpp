# Benchmarks

Benchmarks answer three different questions. Mixing them produces wrong conclusions.

## Three questions

1. **Same quantization** — how much faster the strix engine is than llama.cpp at the same upstream baseline, on types both support (`Q4_0`, `Q4_K_M`, `Q8_0`). This measures the engine, and it is the test for whether kernel work polluted the standard paths.
2. **Same BPW, different format** — strix's 4.50 BPW `Q4_0_ROCMFP4` against upstream's 4.5 BPW `Q4_0`. These must report speed **and** PPL together; speed alone is misleading because quantization quality differs.
3. **Picking a tier on one side** — choosing among `Q4_0_ROCMFP4` / `_FAST` / `_COHERENT` / `_STRIX`. Upstream is not involved; one build is enough.

## Tooling

`tests/parity/` covers questions 1 and 2:

- `cases.json` defines the shapes (pp / tg / reps) once, shared by both sides.
- `run.sh` runs both `llama-bench` binaries over the same shapes, each producing JSONL.
- `compare.py` pairs rows on `(test, n_prompt, n_gen)` and prints a ratio table; `--json` for machine consumption.

Usage and the upstream control build are documented in `tests/parity/README.md`.

`llama-bench` itself emits `-o csv|json|jsonl|md|sql`, so both sides are structurally identical and can share a table — or a single SQLite database.

## Where existing numbers live

- **Per-release measurements**: the matching `CHANGELOG.md` entry. The 2026-10 sync records `Q4_0_ROCMFP4_FAST` on Qwen3.8-27B at pp256 418→742 (+78%), pp2048 399→762 (+91%), tg256 flat.
- **Evidence and reproduction per tuning**: `docs/development/kernel-optimization.md`, including the optimisations that were withdrawn after failing numerical gates.
- **Historical comparison data**: `benches/` (upstream stock; other platforms, not directly comparable here).

This page does not repeat those numbers — they move with every release, and a copy would just be a second set that goes stale.

## Reading the table

- `ratio` > 1 means the strix side is faster.
- **A same-quant `pp` row with `ratio` < 1 beyond the 3-5% noise band is a regression until proven otherwise.** That is the entire point of the harness.
- Cross-format rows must be read with PPL, and each side's BPW stated.
- `tg` rows swing more (sampling, KV type); comparing them alone says little.

## Fairness constraints

Same machine, same ROCm, same gfx1151, same model file, same ctx, `reps` ≥ 5 taking the median; record clocks, temperature and power profile. Keep both sides' `--version` output — this project prints `upstream base`, which is how you prove the control ran the intended commit.

## Upstream documents

- Options and output formats: `tools/llama-bench/README.md`
- Aggregating multiple result sets: `scripts/compare-llama-bench.py`
- Comparing across commits: `scripts/compare-commits.sh`
