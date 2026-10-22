# strix.cpp

**A llama.cpp engine tuned for the AI MAX+ 395, with native ROCmFPX quantization support.**

strix.cpp is an independently maintained branch of llama.cpp. It does three things properly: it keeps in step with upstream; it supports the ROCmFPX quantization family natively (formats upstream cannot load); and it folds in multimodal vision and grounding. What it is really built for, though, is kernel-level tuning on this one chip — Strix Halo (gfx1151), up to 128 GiB of unified memory: same model, same quantization, attention kernels more than 20% faster.

[中文](README.md) · [Changelog](CHANGELOG.md) · [Roadmap](ROADMAP.md) · [Quantization guide](docs/quantization-guide.en.md)

---

## Highlights

- **Quantization formats that just work**: the whole ROCmFP4 / ROCmFPX family (2-bit through 8-bit, including the Agent routing variants), with `llama-quantize` built in. See the [quantization guide](docs/quantization-guide.en.md) for which tier to pick.
- **Multimodal**: vision models and LocateAnything grounding run directly; `--special` emits structured `<ref>/<box>` output.
- **Tuned for this chip**: attention kernels split by shape, prefill GEMM wired to FP4-specific K-tiling, and the per-layer bulk copy in SSM convolution removed. Together they lift end-to-end prefill by 44–73%.
- **One binary**: text, vision and grounding without swapping engines.
- **ROCm only**: HIP is the only backend built, no Vulkan, device pinned to ROCm0 — no ambiguity about which backend you got.

## Benchmarks

**Against upstream llama.cpp** (same machine, model, quantization and flags; Qwen3.8-27B / `Q4_0_ROCMFP4_FAST`)

| Metric | Upstream | strix.cpp | Change |
|:--|--:|--:|--:|
| prefill pp256 | 418 | 742 | **+78%** |
| prefill pp2048 | 399 | 762 | **+91%** |
| generation tg256 | 14.05 | 14.04 | flat |

**Across the matrix** (five models × seven tiers, 35 cells):

- Prefill gains concentrate in the Q4 family: pp256 **+43–83%**, pp2048 **+58–93%**;
- Q6 / Q8 narrows to pp256 **−7% to +33%**, pp2048 **+1% to +31%** — the FP4-specific K-tiling does not apply there;
- Generation stays flat throughout: **−0.1% to +3.1%**. Decoding runs the MMVQ path, which none of these kernel changes touch — a deliberate scope choice, not an oversight.

**Self-evolution** (v1.0.0 → v1.0.2; v1.1.0 left these kernel paths untouched, so the numbers carry over)

| Case (Qwen3.8-27B, Q4_0_ROCMFP4_FAST) | v1.0.0 | v1.0.2 | Change |
|:--|--:|--:|--:|
| prefill pp2048 (short context) | 437 | 757.6 | +73% |
| prefill pp16384 (deep context) | 389 | 638.8 | +64% |
| prefill pp65536 (very deep context) | 295 | 425.8 | +44% |
| generation tg256 | 14.0 | 14.0 | flat |

**Where the gains come from**: attention kernels split by shape (kernel +22.6%); prefill GEMM wired to FP4-specific K-tiling (kernel +90%, +44–73% end to end after dilution); the bulk copy removed from SSM convolution (that copy was 64.6% of kernel time, +5% end to end). Evidence and reproduction steps are in the [kernel optimization record](docs/development/kernel-optimization.en.md).

## Scope

- **Hardware**: AMD Ryzen AI MAX+ 395 (gfx1151 / Radeon 8060S); 128 GiB unified memory recommended.
- **System**: Linux + ROCm 10.x, HIP backend only.
- **Verified models**: the Qwen3.8-27B family (official / abliterated / Coder390); Ornith-1.5-35B-A3B (official / abliterated); vision and LocateAnything via `--special`.
- **Quantization**: the full ROCmFP4 / ROCmFPX family; standard GGUF (Q4_K / Q5_K / Q6_K / Q8_0 and friends) works as usual.

## Quick start

### Build

```bash
export ROCM_PATH=/opt/rocm PATH=/opt/rocm/bin:$PATH
cmake -B build -DGGML_HIP=ON -DCMAKE_BUILD_TYPE=Release -DAMDGPU_TARGETS=gfx1151 \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_PREFIX_PATH=/opt/rocm/core-10.0 -DLLAMA_CURL=OFF -DGGML_OPENMP=OFF
cmake --build build -j $(nproc) --target strix-server llama-quantize llama-bench
```

Artifacts land in `build/bin/`. The executable is `strix-server`; `build/bin/llama-server` is a compatibility symlink pointing at it, so existing scripts keep working.

### One-click install

```bash
curl -fsSL https://raw.githubusercontent.com/ArtomYuan/strix.cpp/master/install.sh | bash
```

By default this creates `strix.cpp/` in the current directory: sources in `strix.cpp/src/`, binaries in `strix.cpp/bin/`, runtime libraries in `strix.cpp/lib/`. Set `INSTALL_DIR=...` to install elsewhere.

From a checkout you can also run `./scripts/install.sh`; the install directory follows the current directory and defaults to `strix.cpp/` beneath it.

After installing, the binary is at `<install dir>/bin/strix-server` and the runtime libraries at `<install dir>/lib/`. The binary carries a `$ORIGIN/../lib` RUNPATH, so `LD_LIBRARY_PATH` is usually unnecessary — set it only when ROCm lives outside the default path.

### Run

```bash
build/bin/strix-server -m <model.gguf> --port 8080
```

OpenAI-compatible API (under `/v1` by default). Add `--special` for vision and grounding models; plain text models do not need it.

### As a service

```ini
[Service]
Type=simple
Environment=HSA_OVERRIDE_GFX_VERSION=11.5.1
Environment=HIP_VISIBLE_DEVICES=0
Environment=LD_LIBRARY_PATH=/path/to/engine/lib:/opt/rocm/lib
ExecStart=/path/to/engine/bin/strix-server --special -m /path/to/model.gguf --port <port>
```

## Speculative decoding (MTP)

When a model ships `nextn` layers, speculative decoding is available: one pass over the weights, several tokens out, past the single-stream bandwidth ceiling.

```bash
build/bin/strix-server -m <model.gguf> --port 8080 --spec-type draft-mtp --spec-draft-n-max 6
```

With the same prompt and seed, output is **byte-identical** to a run with MTP off. The gain depends entirely on draft acceptance, and it varies a lot:

- Early v1.0.2 record (depth 6): generation 14.0 → 37.9 t/s;
- Re-measured on `Q4_0_ROCMFP4_FAST` in 2026-10: 14.0 → 21.9 t/s, 27% acceptance;
- Across five models: dense 27B gains **+56–88%**, while MoE Ornith **loses 18%** (the verification step wakes more experts).

Depth 8 and beyond collapses acceptance to 0.6% — pure overhead, and output no longer matches the baseline. **Off by default; measure on your own model and quantization before enabling.** Parameter sweeps and the mechanism are in [kernel optimization, section 6](docs/development/kernel-optimization.en.md).

## Documentation

- [Quantization guide](docs/quantization-guide.en.md): family table, how to choose, quantize commands, KV types
- [Kernel optimization record](docs/development/kernel-optimization.en.md): evidence and reproduction for every tuning change
- [Install](docs/install.md) · [Multimodal](docs/multimodal.md) · [Model support](docs/models.md) · [Speculative decoding](docs/speculative.md)
- [Roadmap](ROADMAP.md) · [Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md)

## Known limitations

- **Tuned for gfx1151 only**: other ROCm devices build and run, but do not get these kernel gains.
- **Accuracy edge at very long context**: beyond 32k, f16-KV attention drifts from the f32 reference as depth grows (about 3e-3 at 131k). This is inherited llama.cpp behaviour — upstream does the same — and moves model output by a fraction of a percent. Keep it in mind for heavy long-context work.
- **ROCmFPX models must be quantized locally**: no such weights exist upstream; produce them with this repo's `llama-quantize`.
- **MTP needs `nextn` layers in the model**: not every model has them, and the flag does nothing without them.

## License

MIT. Based on [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp).
