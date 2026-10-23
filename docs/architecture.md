# 架构

这篇回答一个问题：**这个库里哪些是上游、哪些是我们，它们怎么接在一起。** 上游的通用构建选项、后端说明不在这里重复，文末给对应文档。

## 两层结构

strix.cpp 是 llama.cpp 的分支，代码分两层，边界是明确的：

| 层 | 位置 | 归属 | 同步行为 |
|:--|:--|:--|:--|
| 上游骨架 | `ggml/`、`src/`、`common/`、`tools/`、`examples/`、`vendor/` | ggml-org/llama.cpp | 每次 merge 更新 |
| 自有层 | `strix/`、`ggml/rocmfp4/`、`ggml/rocmfpx/`、13 个模板实例、`ggml-cuda` 下 30 个改写文件、`tests/parity/`、自有文档 | 本库 | 冲突时取本地 |

上游的 18 个后端子目录全部保留。构建只打开 `GGML_HIP`，其余默认 OFF——`ggml_add_backend()` 里的 `if (GGML_<BACKEND>)` 守卫让未启用的后端连 `add_subdirectory` 都不执行，所以保留它们不产生编译成本，只产生 diff 噪音。

## 自有层的六个部分

- **`strix/cli/`** — 统一入口 `strix`。七个命令转发到同目录二进制，`diagnose` 与 `version` 自己实现。它不改变任何上游 target 名。
- **`ggml/rocmfp4/`、`ggml/rocmfpx/`** — 量化格式定义、CPU 参考实现、HIP 辅助头。上游没有这两个目录，所以它们放在 `ggml/` 下不产生合并冲突。
- **`ggml/src/ggml-cuda/template-instances/` 的 13 个实例** — FA 向量与 MMQ 的特化编译单元（7 个 `fattn-vec-instance-*`、6 个 `mmq-instance-*`）。
- **`ggml/src/ggml-cuda/` 下 30 个改写文件** — 新类型接入所有算子路径的横切改动：MMQ、MMVQ、FlashAttention、convert、getrows/set-rows、7 个架构配置表。
- **`tools/mtmd/models/locateanything.cpp`** — 视觉定位投影器。
- **`tests/parity/`** — 与上游同量化的对比管线。

完整清单与“冲突时取本地”的规则在 [CONTRIBUTING 的「自有资产清单」](../CONTRIBUTING.md#自有资产清单)。

## 一次推理的数据流

1. **GGUF 头** → `llama_model`（`src/llama-model.cpp` 及其加载器）。
2. **张量类型码** → `ggml` 类型表。ROCmFP 族占 100-107，`GGML_TYPE_COUNT` 为 108。
3. **权重上传** → `ggml_backend_*` buffer → HIP 设备内存。
4. **图构建** → `src/models/*.cpp`，共享 `llm_graph_context` 这套通用抽象。
5. **算子分派** → 小批走 MMVQ，大批走 MMQ，都不命中则回退 hipBLAS。
6. **采样与输出** → `common/sampling.cpp`、`tools/server/`。

改量化类型要动 2、3、5 三处；只改内核只动 5。这也解释了为什么“自有改动”是分散的——量化类型是横切能力，不是一个独立模块。

## 为什么 `rocmfp4` 不搬进 `strix/`

搬过一遍的结论是成本更高：这两个目录上游不存在、本来零冲突；而 `ggml.c`、`ggml-quants.c`、`ggml-cpu.c`、`ggml-cuda/common.cuh` 里有 17 处按相对路径 include 它们的头文件，搬走要全部改。另外 `mmq-instance-*` 被 `file(GLOB "template-instances/mmq*.cu")` 自动收，搬走会破坏 GLOB。

## 上游对应文档

- 通用构建选项：`docs/build.md`；本库 preset 与产物布局：`docs/build-presets.md`
- 后端说明：`docs/backend/`
- 新增模型：`docs/development/HOWTO-add-model.md`
- 每项内核特调的证据与复现：`docs/development/kernel-optimization.md`
