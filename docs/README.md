# strix.cpp 文档索引

本索引覆盖 `docs/` 下的全部文档。上游 llama.cpp 的文档原样承接，本库新增或改写的单列在前面。

上游同步基线：`091b6a7fc`（2026-10-14），批次明细见 [上游同步批次记录](upstream-sync.md)。

## 本库自有

- [量化指南](quantization-guide.md) — ROCmFP4 / ROCmFPX 家族表、档位选择、量化命令、KV 类型（[English](quantization-guide.en.md)）
- [内核优化记录](development/kernel-optimization.md) — 每项 Strix Halo 特调的证据、复现方法与回退面（[English](development/kernel-optimization.en.md)）
- [发布流程](release.md) — 版本号口径、tag 与 Release 规范、CI 现状
- [上游同步批次记录](upstream-sync.md) — 每次合并的窗口、冲突面、跳过项与验证数字（中文）
- [路线图](roadmap.md) — 接下来要做什么，以及已知边界（[English](roadmap.en.md)）
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

## 文档放在哪

根目录只放平台或法律要求固定路径的文件，其余说明性文档一律进 `docs/`：

- **根目录固定**：`README.md` / `README.en.md`（GitHub 首页）、`LICENSE` / `LICENSE-MIT` / `NOTICE` / `THIRD_PARTY_NOTICES.md`（许可证）、`CONTRIBUTING.md` / `SECURITY.md` / `CODE_OF_CONDUCT.md`（GitHub 社区健康文件）、`CHANGELOG.md` / `CHANGELOG.en.md`（Keep a Changelog 惯例）、`AGENTS.md` / `CLAUDE.md`（agent 工具读取约定）。
- **`docs/` 收纳本库自有文档**：新增文档必须在本索引登记，没有登记视为未完成。
- **模块 README 随模块走**：`tools/*/README.md`、`ggml/src/*/README.md`、`examples/*/README.md` 是上游文件，保持原位——移动会在每次同步时制造冲突。
- 中文为主文件，英文为同名 `.en.md`，两份同批提交；内部流程文档（如同步批次记录）可只有中文。

其他约定：

- `ggml/rocmfp4/README.md` 是 ROCmFP4 格式的历史设计档案（文中已自标 Historical），当前用法以量化指南为准。
- 上游文档原样承接，内部少量相对路径未修正（例如 `docs/autoparser.md` 指向 `common/` 下的头文件时缺 `../`）。这类断链属于上游遗留，同步时保持原样，不在本库单独改写。
