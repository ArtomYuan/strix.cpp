# Architecture

This page answers one question: **which code is upstream, which code is ours, and how the two connect.** Generic build options and backend notes are not repeated here; the last section points at them.

## Two layers

strix.cpp is a branch of llama.cpp with an explicit boundary:

| Layer | Location | Owner | Sync behaviour |
|:--|:--|:--|:--|
| Upstream skeleton | `ggml/`, `src/`, `common/`, `tools/`, `examples/`, `vendor/` | ggml-org/llama.cpp | replaced by each merge |
| Project layer | `strix/`, `ggml/rocmfp4/`, `ggml/rocmfpx/`, 13 template instances, 30 rewritten files under `ggml-cuda`, `tests/parity/`, project docs | this repository | local side wins on conflict |

All 18 upstream backend directories are kept. The build enables only `GGML_HIP`; the rest default to OFF, and the `if (GGML_<BACKEND>)` guard inside `ggml_add_backend()` means a disabled backend never reaches `add_subdirectory`. Keeping them costs nothing at build time and only adds diff noise.

## The six parts of the project layer

- **`strix/cli/`** — the `strix` entry point. Seven commands forward to sibling binaries; `diagnose` and `version` are implemented in place. No upstream target is renamed.
- **`ggml/rocmfp4/`, `ggml/rocmfpx/`** — format definitions, CPU reference implementations, HIP helper headers. Neither directory exists upstream, so living under `ggml/` creates no merge conflict.
- **13 instances under `ggml/src/ggml-cuda/template-instances/`** — specialised translation units for FA vector and MMQ (7 `fattn-vec-instance-*`, 6 `mmq-instance-*`).
- **30 rewritten files under `ggml/src/ggml-cuda/`** — the cross-cutting work of wiring new types into every operator path: MMQ, MMVQ, FlashAttention, convert, getrows/set-rows, and the seven architecture config tables.
- **`tools/mtmd/models/locateanything.cpp`** — the visual grounding projector.
- **`tests/parity/`** — the same-quantization comparison harness against upstream.

The full list and the "local side wins" rule live in [CONTRIBUTING, 自有资产清单](../CONTRIBUTING.md#自有资产清单).

## Data flow of one inference

1. **GGUF header** → `llama_model` (`src/llama-model.cpp` and its loaders).
2. **Tensor type codes** → the `ggml` type table. The ROCmFP family occupies 100-107; `GGML_TYPE_COUNT` is 108.
3. **Weight upload** → `ggml_backend_*` buffers → HIP device memory.
4. **Graph construction** → `src/models/*.cpp`, sharing the `llm_graph_context` abstraction.
5. **Operator dispatch** → MMVQ for small batches, MMQ for large ones, hipBLAS as the fallback.
6. **Sampling and output** → `common/sampling.cpp`, `tools/server/`.

Changing a quantization type touches steps 2, 3 and 5; changing a kernel touches only step 5. This is also why project changes are spread out: a quantization type is a cross-cutting capability, not a standalone module.

## Why `rocmfp4` is not moved into `strix/`

It was measured: both directories do not exist upstream, so they already merge conflict-free. Moving them would require editing 17 relative include sites in `ggml.c`, `ggml-quants.c`, `ggml-cpu.c` and `ggml-cuda/common.cuh`, and would break the `file(GLOB "template-instances/mmq*.cu")` rule that picks up the `mmq-instance-*` files.

## Upstream documents

- Generic build options: `docs/build.md`; project presets and artefact layout: `docs/build-presets.md`
- Backend notes: `docs/backend/`
- Adding a model: `docs/development/HOWTO-add-model.md`
- Evidence and reproduction for each kernel tuning: `docs/development/kernel-optimization.md`
