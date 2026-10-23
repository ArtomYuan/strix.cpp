# Roadmap

This document describes **what we plan to do next**. For what has already shipped, see [CHANGELOG.en.md](../CHANGELOG.en.md); item-by-item evidence lives in the kernel optimization record and in each release's notes.

Current version: **v1.1.0 (2026-10-19, strix-server rename + spec-batch contiguity check)**; the GEMM/MoE/SSM changes shipped in v1.0.1

The roadmap lists work in progress, not commitments — scope and order may change. What gets done first, and whether it gets done at all, is the maintainer's call.

---

## Next batch

### Fused activation quantization: drop the per-layer round trip

**Area: prefill path (RMSNorm → MMQ)**

Every RMSNorm writes its result back to VRAM as F32, and MMQ reads it back to quantize. At batch 2048 that is a couple of hundred MB per layer. What gufo does is have the norm's exit write the quantized tile directly.

- Fuse it at the graph level; validate on a single model and layer first
- Byte-for-byte equivalence before any end-to-end A/B
- Keep the retreat wide: the fused op must be switchable off

> One less trip beats a faster trip.

### Prefill GEMM tiles and routing: push 53 higher

**Area: prefill GEMM (all dense layers)**

The K tile already lifted the main shapes to 53 TFLOPS, but the tile layout is still the generic dispatch. gufo's published 30–34 TF was their old number; our target is 60+.

- Re-tile for the 27B shape family (I=128/256, J=128)
- Evaluate whether stream_k is worth it
- Keep it only if end-to-end A/B does not regress

> Generic is for everyone; tuning is for this one chip.

### Long-context decode: assess before promising

**Area: decode (MMVQ / KV path)**

Decode is currently level with the previous version. gufo's 1.7× comes from KV tiles shared across heads plus sparse splitting, but that is their model's structure — whether it holds for 27B/Ornith needs to be costed first.

- Assess the gain and the VRAM cost; write a one-page conclusion
- Schedule it if it holds; record a rejection if it does not
- Do not experiment on the live decode path

> Measure first, then move.

---

## Decide with us

- **Production switch**: v1.1.0 (commit `309bfef16`) → `<install-dir>/bin/strix-server`, via full mirror + smoke test + rollback plan.
- **Release**: v1.0.1 through v1.1.0 are tagged and released; next is scope and timing for v1.2.0.
- **What runs first**: wiring work for fused activation quantization, prefill tile/routing, or the long-context assessment — the maintainer's call.

---

## Known boundaries (current version)

- Deep-context precision edge: beyond 32k tokens, f16-KV attention slowly drifts from the f32 reference (about 3e-3 at 131k); the same on upstream.
- fp8 family K=512 has been verified: numerically correct (rocmfpx 61/61) but slower; on RDNA3.5 the iu8 matrix-core throughput is half of iu4 (25.3 vs 51.7 TFLOPS), so K=256 stays. See `docs/development/kernel-optimization.en.md` section 5.
