# Third-Party Notices

This repository is based on `llama.cpp`. Upstream code remains under the MIT
license (`LICENSE-MIT`, Copyright (c) 2023-2026 The ggml authors). New
strix.cpp code is licensed under the Apache License 2.0 (`LICENSE`). The
third-party license files that ship with the tree are unchanged.

## Pinned versions

为什么有这张表：它回答「这份代码基于哪个上游点」，不是许可证形式主义。每次同步只改一行。

| 组件 | 关系 | 许可 | 固定版本 / revision | 来源 |
|:--|:--|:--|:--|:--|
| llama.cpp / ggml | 改编来源，整仓基线 | MIT | `091b6a7fc`（`upstream.txt`） | [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp) |
| ROCmFP4 / ROCmFPx | 格式定义与参考实现 | MIT | unrecorded（见下） | [charlie12345/ROCmFPX](https://github.com/charlie12345/ROCmFPX) |
| ROCm / HIP | 链接运行时 | MIT | 10.0.0（`/opt/rocm`） | [ROCm](https://github.com/ROCm) |
| hipCUB / rocPRIM / rocWMMA | 头文件与内核 | MIT / BSD-3-Clause | 随 ROCm 10.0.0 | [ROCm](https://github.com/ROCm) |
| cpp-httplib / nlohmann-json / stb | 随上游打包 | 各自许可 | 随 llama.cpp 基线 | 见各头文件 |

ROCmFPX 的导入 commit 没有记录——当时的迁移只写进了 CHANGELOG 的叙述（「按提取法移植」），没有留 sha。这条记为 `unrecorded`，不补造。

## ROCmFP4 / ROCmFPx formats

- Source: https://github.com/charlie12345/ROCmFPX
- License: MIT (identical text and copyright line to this repository's `LICENSE-MIT`)
- Taken over as-is: `ggml/rocmfp4/` and `ggml/rocmfpx/` (format definitions,
  CPU reference quantizers/dequantizers, HIP helper headers, reference tests),
  the CPU vec-dot kernels in `ggml/src/ggml-cpu/ggml-cpu.c`, the HIP
  dequant/copy/FlashAttention integration across `ggml/src/ggml-cuda/`, the
  quantization recipes in `src/llama-quant.cpp`, and the TurboQuant KV-cache
  type definitions that travel with them.
- Re-expressed here rather than copied: the FP4 MMQ load-tile kernels
  (`mmq-load-tiles.cuh`, `mmq-config-*.cuh` entries), which the source tree
  implements against the pre-refactor MMQ architecture.

The upstream `llama.cpp` bundled components (cpp-httplib, nlohmann/json, stb,
etc.) are unchanged from upstream; see their respective headers.
