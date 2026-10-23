# strix.cpp

**为 AI MAX+ 395 深度特调的 llama.cpp 引擎，原生支持 ROCmFPX 量化。**

strix.cpp 是 llama.cpp 的**分支**，不是独立引擎：上游代码是主体，本库的改动集中在 ROCmFP 量化族、多模态视觉与 Strix Halo 内核特调，与上游保持同步是首要承诺。它把三件事做扎实：持续同步官方上游；原生支持 ROCmFPX 量化（上游加载不了的格式）；合并多模态视觉与定位能力。而它真正的立身之本，是在这颗芯片——Strix Halo（gfx1151）、最多 128 GiB 统一内存——上做内核级特调：同样的模型、同样的量化，注意力内核快两成以上。

[English](README.en.md) · [更新记录](CHANGELOG.md) · [路线图](docs/roadmap.md) · [量化指南](docs/quantization-guide.md)

---

## 特点

- **量化格式开箱即用**：ROCmFP4 / ROCmFPX 全家族（2-bit 到 8-bit、Agent 路由变体），自带 `llama-quantize`。档位怎么选见[量化指南](docs/quantization-guide.md)。
- **多模态**：视觉模型与 LocateAnything 定位模型直接可用；`--special` 输出 `<ref>/<box>` 结构化框。
- **为这块芯片调过**：注意力内核按形状分裂配置；预填 GEMM 接上 FP4 专用 K 分块；SSM 卷积去掉每层一次的整块拷贝。三项合计把整网预填推高 44~73%。
- **一个二进制**：文本、视觉、定位不换引擎。
- **纯 ROCm**：只编 HIP、无 Vulkan、设备锁定 ROCm0，没有后端选择歧义。

## 实测

**对上游 llama.cpp**（同机、同模型、同量化、同参数；Qwen3.8-27B / `Q4_0_ROCMFP4_FAST`）

| 指标 | 上游 | strix.cpp | 变化 |
|:--|--:|--:|--:|
| 预填 pp256 | 418 | 742 | **+78%** |
| 预填 pp2048 | 399 | 762 | **+91%** |
| 生成 tg256 | 14.05 | 14.04 | 持平 |

**截面分布**（五模型 × 七档，共 35 组）：

- 预填收益集中在 Q4 家族：pp256 **+43~83%**、pp2048 **+58~93%**；
- Q6 / Q8 家族收益收窄：pp256 **−7~+33%**、pp2048 **+1~+31%**（FP4 专属的 K 分块在这些档位不生效）；
- 生成侧全程持平：**−0.1~+3.1%**。解码走 MMVQ 路径，不在本轮内核改动范围内——这是设计取向，不是遗漏。

**自我演进**（v1.0.0 → v1.0.2；v1.1.0 未改这些内核路径，数字沿用）

| 场景（Qwen3.8-27B，Q4_0_ROCMFP4_FAST） | v1.0.0 | v1.0.2 | 变化 |
|:--|--:|--:|--:|
| 预填 pp2048（短上下文） | 437 | 757.6 | +73% |
| 预填 pp16384（深上下文） | 389 | 638.8 | +64% |
| 预填 pp65536（超深上下文） | 295 | 425.8 | +44% |
| 生成 tg256 | 14.0 | 14.0 | 持平 |

**三刀分别贡献了什么**：注意力内核按形状分裂配置（内核 +22.6%）；预填 GEMM 接上 FP4 专用 K 分块（内核 +90%，整网摊薄后 +44~73%）；SSM 卷积去掉每层一次的整块拷贝（该拷贝原占内核 64.6% 的时间，整网 +5%）。证据与复现见[内核优化记录](docs/development/kernel-optimization.md)。

## 支持范围

- **硬件**：AMD Ryzen AI MAX+ 395（gfx1151 / Radeon 8060S），建议 128 GiB 统一内存。
- **系统**：Linux + ROCm 10.x，仅 HIP 后端。
- **已验证模型**：Qwen3.8-27B 家族（官方 / abliterated / Coder390）；Ornith-1.5-35B-A3B（官方 / abliterated）；视觉与 LocateAnything 走 `--special`。
- **量化**：ROCmFP4 / ROCmFPX 全家族；标准 GGUF（Q4_K / Q5_K / Q6_K / Q8_0 等）照常可用。

## 快速开始

### 构建

```bash
export ROCM_PATH=/opt/rocm PATH=/opt/rocm/bin:$PATH
cmake --preset hip-gfx1151
cmake --build --preset hip-gfx1151
```

产物在 `build/hip-gfx1151/bin/`。可执行文件是 `strix-server`；`build/hip-gfx1151/bin/llama-server` 是指向它的兼容软链，旧脚本可以直接用。

### 一键安装

```bash
curl -fsSL https://raw.githubusercontent.com/ArtomYuan/strix.cpp/master/install.sh | bash
```

默认在当前目录创建 `strix.cpp/`：源码在 `strix.cpp/src/`，二进制在 `strix.cpp/bin/`，运行库在 `strix.cpp/lib/`。`INSTALL_DIR=...` 可以改安装位置。

仓库内也可以直接跑 `./scripts/install.sh`：安装目录由执行时的当前目录决定，默认落在当前目录下的 `strix.cpp/`。

安装后二进制在 `<安装目录>/bin/strix-server`，运行库在 `<安装目录>/lib/`。二进制带 `$ORIGIN/../lib` 的 RUNPATH，通常不需要再设 `LD_LIBRARY_PATH`；ROCm 不在系统默认路径时再补。

### 一个入口

`strix` 把常用命令收成子命令，参数原样透传给对应的二进制：

```bash
strix serve -m <模型.gguf> --port 8080    # 等价于 strix-server
strix run -m <模型.gguf> -p "你好"         # 等价于 llama-cli
strix bench -m <模型.gguf>                 # 等价于 llama-bench
strix diagnose -m <模型.gguf>              # 检查 ROCm、GPU、二进制与模型
```

上游二进制名一个没改，直接调用仍然有效。完整说明见 [命令行](docs/cli.md)。

### 运行

```bash
build/hip-gfx1151/bin/strix-server -m <模型.gguf> --port 8080
```

OpenAI 兼容 API（默认 `/v1`）。视觉/定位模型加 `--special`；纯文本模型不需要。

### 作为服务常驻

```ini
[Service]
Type=simple
Environment=HSA_OVERRIDE_GFX_VERSION=11.5.1
Environment=HIP_VISIBLE_DEVICES=0
Environment=LD_LIBRARY_PATH=/path/to/engine/lib:/opt/rocm/lib
ExecStart=/path/to/engine/bin/strix-server --special -m /path/to/model.gguf --port <port>
```

## 投机解码（MTP）

模型自带 `nextn` 层时可以开投机解码：一次读权重、产出多个 token，越过单流带宽上限。

```bash
build/hip-gfx1151/bin/strix-server -m <模型.gguf> --port 8080 --spec-type draft-mtp --spec-draft-n-max 6
```

同提示、同 seed 下，输出与不开启时**逐字一致**。收益完全取决于草稿接受率，实测分散得厉害：

- 早期 v1.0.2 记录（深度 6）：解码 14.0 → 37.9 t/s；
- 2026-10 在 `Q4_0_ROCMFP4_FAST` 上复测：14.0 → 21.9 t/s，接受率 27%；
- 五模型复测：密集 27B 为 **+56~88%**，MoE 的 Ornith 为 **−18%**（验证步唤醒更多专家）。

深度 ≥8 会让接受率塌到 0.6%，只剩草稿开销，且输出不再与基线一致，不要用。**默认关闭，按模型与量化实测后再开。** 参数扫描与原理见[内核优化记录](docs/development/kernel-optimization.md)第 6 节。

## 文档

- [完整文档索引](docs/README.md)：本库自有文档与上游文档分组
- [量化指南](docs/quantization-guide.md)：家族表、怎么选、量化命令、KV 类型
- [内核优化记录](docs/development/kernel-optimization.md)：每项特调的证据与复现
- [安装](docs/install.md) · [多模态](docs/multimodal.md) · [模型适配](docs/models.md) · [投机解码](docs/speculative.md)
- [路线图](docs/roadmap.md) · [更新记录](CHANGELOG.md) · [贡献指南](CONTRIBUTING.md)

## 已知限制

- **仅针对 gfx1151 调优**：其它 ROCm 设备可以编译运行，但拿不到这些内核收益。
- **超长上下文的精度边缘**：f16 KV 的注意力在 32k 以上时，与 f32 参考的误差会随深度缓慢增长（131k 时约 3e-3）。这是 llama.cpp 的既有行为（上游同样如此），对模型输出影响约百分之零点几——超长上下文的重负载场景请留意。
- **ROCmFPX 模型需自行量化**：上游生态没有对应权重，用本库的 `llama-quantize` 产出。
- **MTP 依赖模型自带 nextn 层**：不是所有模型都有，没有时该参数无效。

## 许可

strix.cpp 新增代码采用 **Apache License 2.0**；基于 [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp) 的上游代码仍为 **MIT**，完整文本见 `LICENSE-MIT`。第三方组件见 `THIRD_PARTY_NOTICES.md`。
