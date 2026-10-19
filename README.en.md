# strix.cpp

**A llama.cpp engine deeply tuned for AI MAX+ 395, ROCmFPX-native.**

strix.cpp is an independently maintained llama.cpp branch. It does three things well: it continuously syncs upstream; it natively supports ROCmFPX quantization (formats upstream cannot load); and it folds in multimodal vision. But its reason to exist is kernel-level specialization for this one chip — Strix Halo (gfx1151), up to 128 GiB of unified memory: the same model and the same quant, with an attention kernel that runs over twenty percent faster.

[中文](README.md)

---

## Highlights

- **Quantization that just works**: the full ROCmFP4 / ROCmFPX family (2-bit through 8-bit, plus agent-routing variants) with `llama-quantize` included. See the [quantization guide](docs/quantization-guide.en.md).
- **Multimodal**: vision models and LocateAnything grounding models work out of the box (`--special` emits `<ref>/<box>`).
- **Tuned for this chip**: per-shape attention-kernel configs, plus the FP4-specific K tile on the prefill GEMM — 44~73% faster prefill end to end (method in the [kernel optimization record](docs/development/kernel-optimization.en.md)).
- **One binary**: text, vision and grounding models without switching engines.
- **Pure ROCm**: HIP-only, no Vulkan, device locked to ROCm0 — no backend ambiguity.

## Measured performance

Same machine, same model, same quant, same flags; measured at the v1.0.2 release (v1.1.0 does not touch these kernel paths, so the numbers carry over), control arm `v1.0.0`:

| Scenario (Qwen3.8-27B, Q4_0_ROCMFP4_FAST) | v1.0.0 | v1.0.2 | Delta |
|:--|--:|--:|--:|
| Prefill pp2048 (short context) | 437 t/s | **757.6 t/s** | **+73%** |
| Prefill pp16384 (deep context) | 389 t/s | **638.8 t/s** | **+64%** |
| Prefill pp65536 (very deep) | 295 t/s | **425.8 t/s** | **+44%** |
| Decode tg256 | 14.0 t/s | 14.0 t/s | flat (decode takes a different path) |
| Decode @ depth 16k | 12.69 t/s | 12.69 t/s | flat |
| Decode @ depth 64k | 9.714 t/s | 9.710 t/s | flat |

(The other quant tiers: `Q8_0_ROCMFPX` and `Q6_0_ROCMFPX` are outside this change and match their previous records; `Q4_0_ROCMFP4_STRIX` belongs to the same ROCmFP4 family but was not re-measured on its own.)

**How to read this**: the prefill wins come from three cuts — per-shape attention-kernel configs (**+22.6%** in the kernel itself), the FP4-specific K tile on the prefill GEMM (**+90%** in the kernel, still +44~73% after dilution by the whole network), and removing the per-layer copy in the SSM convolution (that copy was 64.6% of kernel time; +5% end to end). Decode is unchanged in both versions: it takes the MMVQ path, which this change does not touch. Item-by-item evidence is in the [kernel optimization record](docs/development/kernel-optimization.en.md).

## Quickstart

### Build

```bash
export ROCM_PATH=/opt/rocm PATH=/opt/rocm/bin:$PATH
cmake -B build -DGGML_HIP=ON -DCMAKE_BUILD_TYPE=Release -DAMDGPU_TARGETS=gfx1151 \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_PREFIX_PATH=/opt/rocm/core-10.0 -DLLAMA_CURL=OFF -DGGML_OPENMP=OFF
cmake --build build -j $(nproc) --target strix-server llama-quantize llama-bench
```

Artifacts land in `build/bin/`. The executable is `strix-server`; `build/bin/llama-server` is a compatibility symlink to it, so old scripts keep working.

### Run

```bash
build/bin/strix-server -m <model.gguf> --port 8080
```

OpenAI-compatible API (default `/v1`). Vision/grounding models need `--special`; plain text models do not.

When the model ships a `nextn` layer, speculative decoding is available (only Qwen3.8-27B is verified so far):

```bash
build/bin/strix-server -m <model.gguf> --port 8080 --spec-type draft-mtp --spec-draft-n-max 6
```

With the same prompt and seed the output is **byte-identical** to a non-speculative run, while generation goes from 14.0 to 37.9 t/s. Draft depth 8 and beyond collapses the acceptance rate to 0.6% and only adds overhead; do not use it. With multiple concurrent sequences decoding batches normally; prompt and decode batches are never mixed, since their per-sequence token counts differ. See [kernel optimization, sections 6 and 8](docs/development/kernel-optimization.en.md); since v1.1.0 `process()` also rejects interleaved sequence batches, see CHANGELOG `[v1.1.0]`.

### Keep it running as a service

```ini
[Service]
Type=simple
Environment=HSA_OVERRIDE_GFX_VERSION=11.5.1
Environment=HIP_VISIBLE_DEVICES=0
Environment=LD_LIBRARY_PATH=/path/to/engine/bin
ExecStart=/path/to/engine/bin/strix-server --special -m /path/to/model.gguf --port <port>
```

## Documentation

- [Quantization guide](docs/quantization-guide.en.md): family tables, how to choose, quantize commands, KV types
- [Kernel optimization record](docs/development/kernel-optimization.en.md): evidence and reproduction for every kernel specialization
- [Roadmap](ROADMAP.en.md): what comes next and why, and which item runs first
- [Contributing](CONTRIBUTING.md) (Chinese)

## Known limitations

- **Deep-context precision edge**: beyond 32k tokens, f16-KV attention slowly drifts from the f32 reference (about 3e-3 at 131k). This is pre-existing llama.cpp behaviour (the same on upstream) and amounts to roughly a few tenths of a percent of model output — keep it in mind for very-long-context heavy workloads.
