# 贡献指南

感谢你考虑为 **strix.cpp** 做出贡献！🎉

本项目是面向 **AMD Strix Halo**（Ryzen AI MAX+ 395 / Radeon 8060S / gfx1151）的融合引擎：官方 llama.cpp master + **ROCmFPX 量化格式** + **多模态视觉塔** 三合一。

欢迎各种形式的贡献：

- 🐛 报告 bug（构建失败、量化错误、推理异常）
- 💡 提出功能建议
- 📝 修正或补充文档
- 🔧 提交代码 PR

---

## 开发环境

本库的构建目标只有 ROCm HIP（gfx1151）。上游的其他后端（Vulkan、CUDA、SYCL、OpenCL、Hexagon 等）源码完整保留在 `ggml/src/` 下，但默认全部关闭，构建命令只显式打开 `GGML_HIP`——保留与裁剪的取舍见「[上游同步](#上游同步)」。前置要求：

- ROCm 10.0.0（安装于 `/opt/rocm`）
- gcc / g++、cmake（≥ 3.14）、git

构建命令（与 README 一致）：

```bash
export ROCM_PATH=/opt/rocm PATH=/opt/rocm/bin:$PATH
cmake -B build -DGGML_HIP=ON -DCMAKE_BUILD_TYPE=Release -DAMDGPU_TARGETS=gfx1151 \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_PREFIX_PATH=/opt/rocm/core-10.0 -DLLAMA_CURL=OFF -DGGML_OPENMP=OFF
cmake --build build -j $(nproc) --target strix-server llama-quantize llama-bench
```

产物在 `build/bin/`（strix-server / llama-quantize / llama-bench + 共享库）。

## 提交规范

提交信息使用 **Conventional Commits** 风格，格式：`<type>: <描述>`（描述用中文，简洁说明改了什么、为什么）。

| 类型 | 用途 | 示例 |
|:--|:--|:--|
| `feat:` | 新功能 | `feat: 支持 Q4_0_ROCMFP4_STRIX 量化类型` |
| `fix:` | 修复 bug | `fix: 修复 HIP 后端在 gfx1151 上的对齐崩溃` |
| `docs:` | 文档 | `docs: 更新量化格式列表` |
| `chore:` | 构建/工具/杂项 | `chore: 清理 CMake 缓存` |
| `refactor:` | 重构（不改变行为） | `refactor: 简化视觉塔投影器代码` |

要求：

- 一次提交只做一件事
- 提交信息描述行为变化，而不是复述 diff
- 保持与官方 llama.cpp 同步时使用上游原始提交信息，不要重写

## 分支与 PR 流程

1. 从最新的 `master` 切出功能分支：`feat/xxx`、`fix/xxx`、`docs/xxx`
2. 修改代码，本地构建验证（见「测试」）
3. 按提交规范提交
4. push 到你的 fork
5. 在 GitHub 上向本仓库 `master` 发起 Pull Request，描述改动内容并关联相关 issue（如有）

PR 提交前请自查：

- [ ] 本地构建通过（Release + gfx1151）
- [ ] 改动聚焦单一目的，没有夹带无关修改
- [ ] 新代码与周边风格一致
- [ ] 涉及量化类型/视觉模型时，已附实测或验证结果

## 代码风格

- C++ 遵循官方 llama.cpp 的代码风格：与周边代码保持一致，不引入新的模式
- 注释简洁：只解释「为什么」，不解释「做了什么」
- 不硬性换行、不用花哨的 Unicode 字符
- 优先复用现有基础设施，避免引入新的子系统

## 测试

本库只有两个最小 workflow：`.github/workflows/build.yml`（CPU 编译检查）与 `docs.yml`（文档相对链接检查）。官方 CI 矩阵已移除，本地 gfx1151 构建验证仍是主要手段。改动提交前至少完成：

1. **构建通过**：上面的 cmake 构建命令无错误
2. **量化验证**（涉及量化代码时）：

   ```bash
   build/bin/llama-quantize --allow-requantize <源模型.gguf> <输出.gguf> Q4_0_ROCMFP4_FAST 32
   ```

3. **推理冒烟测试**：

   ```bash
   build/bin/strix-server -m <量化后模型.gguf> --port 8080
   # 发一条基本请求，确认能正常生成
   ```

4. **内核路径改动**：另行完成 `test-backend-ops`（MUL_MAT 按类型过滤）与计数器级复核——完整验证链与复现命令见 [docs/development/kernel-optimization.md](docs/development/kernel-optimization.md)（内核优化记录）。

## 上游同步

本库跟踪官方 `ggml-org/llama.cpp`。当前同步基线与历次批次的窗口、跳过项、验证数字见 [docs/upstream-sync.md](docs/upstream-sync.md)（2026-10 为 `091b6a7fc`）。同步是独立工作：用 `sync/upstream-YYYYMMDD` 分支，不夹带功能改动。

### 边界原则

- **保留上游全部后端目录，不裁剪。** `ggml/src/` 下的 18 个后端子目录（`ggml-cuda`、`ggml-vulkan`、`ggml-sycl`、`ggml-opencl`、`ggml-hexagon` 等）与上游保持一致。裁剪会在每次同步时制造 `deleted by us` 冲突，成本永久化。
- **构建只开 HIP。** `ggml_add_backend()` 用 `if (GGML_<BACKEND>)` 守卫，未启用的后端连 `add_subdirectory` 都不执行，不产生编译成本；默认值全部 OFF，构建命令只传 `-DGGML_HIP=ON`。
- **`LLAMA_VERSION_*` 不因本库发版而改动。** 它表示上游同步点；strix.cpp 的发布版本在 `STRIX_VERSION`。

### 冲突分级

按改动落点分三档处理，不要一视同仁。

1. **逐文件核对：`ggml/src/ggml-cuda/`。** HIP 后端是薄壳，ROCm kernel 全部复用这个目录。本库在此处有自有资产：12 个 `template-instances/`（fattn-vec 与 mmq 的 q2/q3/q6/q8 rocmfpx、q4_0_rocmfp4、q4_0_rocmfp4_fast、turbo3_0、turbo4_0），以及 `rocmfp4_hip.cu`、`rocmfp4_hip_codebook.cuh`、`rocmfp4_hip_scale.cuh`。上游对该目录的每处改动都要人工判断是否影响这些文件。
2. **核对改动点：`ggml/src/ggml-hexagon/`。** 本库只改过 `htp/act-ops.c` 与 `htp/htp-tensor.c`，其余随上游。
3. **透明合入：其余非 ROCm 后端。** 以 2026-10 基线为例，上游独有文件 39 个：hexagon 21、opencl 8、cuda 4、zdnn 4、metal 1、rpc 1（数字随同步变化，按下面的命令重新归类）。这些后端不参与构建，合入后只做冲突归因，不做功能验证。

```bash
# 本次同步的改动面，按目录归类
git diff --stat upstream/master...HEAD -- ggml/src
# 必须逐文件看的两个目录
git diff --name-only upstream/master...HEAD -- ggml/src/ggml-cuda ggml/src/ggml-hexagon
```

### 验收

- `test-backend-ops -p rocmfp4` 24/24、`-p rocmfpx` 61/61、默认 MUL_MAT 2534/2534（数字以 `CHANGELOG.md` 上一版记录为准）
- gfx1151 HIP 全量构建无错误
- `strix-server --version` 的 `upstream base` 与实际合并的提交一致

### 回滚点

merge 之前打 tag `pre-sync-YYYYMMDD`。同步分支只做同步，功能改动另开分支。

## 沟通

- 提 issue 请使用对应模板（Bug 报告 / 功能请求）
- 本仓库默认语言为**中文**（与 README 一致）
- 建议先在 issue / 讨论中沟通想法，再投入大规模代码工作

再次感谢你的贡献！

## 版本与发布

- 版本号：`vMAJOR.MINOR.PATCH`。
- GitHub Release 名称与 CHANGELOG 标题统一为 `vX.Y.Z (YYYY-MM-DD)`。
- git tag 只用版本号，不附带日期。
