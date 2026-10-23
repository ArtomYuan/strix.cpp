# strix.cpp 文档索引

本索引覆盖 `docs/` 下的全部文档。上游 llama.cpp 的文档原样承接，本库新增或改写的单列在前面。

上游同步基线：`091b6a7fc`（2026-10-14，见 `CHANGELOG.md` 最近一条同步记录）。

## 本库自有

- [量化指南](quantization-guide.md) — ROCmFP4 / ROCmFPX 家族表、档位选择、量化命令、KV 类型（[English](quantization-guide.en.md)）
- [内核优化记录](development/kernel-optimization.md) — 每项 Strix Halo 特调的证据、复现方法与回退面（[English](development/kernel-optimization.en.md)）
- [发布流程](release.md) — 版本号口径、tag 与 Release 规范、CI 现状
- [安装](install.md) — 从源码构建与安装后的目录布局

## 上游文档（本库有改写）

- [构建](build.md)
- [投机解码](speculative.md)（含本库 MTP 实测数据）
- [后端：SYCL](backend/SYCL.md) · [zDNN](backend/zDNN.md) · [Snapdragon 开发](backend/snapdragon/developer.md)

## 上游文档（原样承接）

### 构建与部署

[build.md](build.md) · [build-profiling.md](build-profiling.md) · [build-riscv64-spacemit.md](build-riscv64-spacemit.md) · [build-s390x.md](build-s390x.md) · [docker.md](docker.md) · [xcframework.md](xcframework.md) · [completions.md](completions.md)

### 运行与服务

[models.md](models.md) · [multimodal.md](multimodal.md)（另有 [multimodal/](multimodal/) 各模型说明）· [function-calling.md](function-calling.md) · [autoparser.md](autoparser.md) · [multi-gpu.md](multi-gpu.md) · [preset.md](preset.md) · [llguidance.md](llguidance.md) · [android.md](android.md)

### 后端

[backend/](backend/) — AOCL、BLIS、CANN、CUDA-FEDORA、ET、OPENCL、OPENVINO、SYCL、VirtGPU、ZenDNN、zDNN、[snapdragon/](backend/snapdragon/)

### 开发

[development/HOWTO-add-model.md](development/HOWTO-add-model.md) · [development/debugging-tests.md](development/debugging-tests.md) · [development/parsing.md](development/parsing.md) · [development/token_generation_performance_tips.md](development/token_generation_performance_tips.md) · [development/llama-star/](development/llama-star/)（历史设计档案）

### 运维数据

[ops.md](ops.md) 与 [ops/](ops/) 下的各后端 CSV — 上游 CI 采集的编译与测试耗时数据，约 38 MB。

## 约定

- 中文为主文件，英文为同名 `.en.md`；新增文档两份同批提交。
- 本库自有文档写在 `docs/` 顶层或 `docs/development/`，不放进 `ggml/`、`tools/` 等代码目录。
- `ggml/rocmfp4/README.md` 是 ROCmFP4 格式的历史设计档案（文中已自标 Historical），当前用法以量化指南为准。
- 上游文档原样承接，内部少量相对路径未修正（例如 `docs/autoparser.md` 指向 `common/` 下的头文件时缺 `../`）。这类断链属于上游遗留，同步时保持原样，不在本库单独改写。
