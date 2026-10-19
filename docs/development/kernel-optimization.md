# 内核优化记录（ROCmFPX / ROCmFP4）

[English](kernel-optimization.en.md)

本库（strix.cpp）对 ROCmFPX / ROCmFP4 量化家族在 HIP（gfx1151）上**内核级优化**的记录：每条含问题、定位证据、根因、修复与验证数据，均可按文内命令复现。

---

## 1. 热路径 ue4m3 尺度解码无分支化（v2026.9.22）

### 1.1 问题

`Q8_0_ROCMFPX` 在 gfx1151 上的 prefill 比标准 `Q8_0` 慢约 9–11%（同条件 pp2048：**1107 vs 1235 t/s**）。此前推测与「33 字节块步长 / SRAM 布局 / 访存对齐」有关。

### 1.2 定位（rocprofv3 两级）

**第一级 · 逐内核采样**（`--kernel-trace`，两侧同条件）：

- 两档的 GPU 时间差 **100% 落在 `mul_mat_q`（MMQ）主核**内（每次调用 +11%）；注意力、归一化、量化、搬运等内核逐项相同。
- 两档内核调用数一致；VGPR（232）/SGPR/工作组与网格配置一致 → 排除「多做步骤」与「占用率」两类原因。

**第二级 · 硬件计数器**（`--pmc`，仅采 `mul_mat_q`；旧 fp8 内核 vs std）：

| 计数器 | fp8/std | 读法 |
|:--|:--|:--|
| `SQ_INSTS_SALU` | **1.389** | 标量指令偏多 |
| `SQ_INSTS_BRANCH` | **1.402** | 分支偏多 |
| `SQ_WAIT_ANY` | **1.860** | 等待周期暴涨 |
| `SQ_WAVE_CYCLES` | **1.177** | 同工作量耗时更长 |
| `SQ_INSTS_FLAT` / `SQC_DCACHE_REQ` / `GL2C_MC_RDREQ` / LDS 冲突 | 1.000 | **访存侧完全一致** |

结论：非访存 / 带宽 / 对齐问题；差距集中在**解码路径的标量指令与分支**造成的流水线停顿。

### 1.3 根因

`ggml/rocmfp4/rocmfp4_hip_scale.cuh` 中两个 ue4m3 尺度解码函数（`rocmfpx_ue4m3_to_fp32_finite` 与 `rocmfp4_ue4m3_to_fp32_half_finite`）带**数据相关 if 分支**，且位于 MMQ 装载器、vec_dot、cpy-utils 的**每权重块每线程**热路径上。

```cpp
// 修复前（rocmfpx_ue4m3_to_fp32_finite 节选）
if (x > 0x7e) { return 0.0f; }
const int exp = (x >> 3) & 0xF;
const int man = x & 0x7;
if (exp == 0) { return (float) man * (1.0f / 1024.0f); }
```

### 1.4 修复（提交 `0347fac53`）

改为无分支选择（三元选择 → SEL/CSEL 类选择指令），逐位等价：

```cpp
// 修复后
const int exp = (x >> 3) & 0xF;
const int man = x & 0x7;
const uint32_t bits = ((uint32_t) exp + 119u) << 23 | ((uint32_t) man << 20);
const float    norm = rocmfp4_u32_as_f32(bits);
const float    subn = (float) man * (1.0f / 1024.0f);
const float    val  = (exp == 0) ? subn : norm;
return (x > 0x7e) ? 0.0f : val;
```

一处修改同时覆盖 fp8（Q2/Q3/Q6/Q8）与 fp4 两个家族的全部热路径。

### 1.5 验证

- **位等价**：两个函数改前 / 改后，256 个输入全枚举逐一比对一致（数值零变化）。
- **内核测试**：`test-backend-ops` MUL_MAT `rocmfpx` **60/60**、`rocmfp4` **24/24**。
- **计数器复核**：新 fp8 内核与 std 在所有采集计数器上**逐项打平**（SALU / 分支 / 等待 = 1.00x、波周期 0.991）；fp4 MMQ 核波周期 −8.6%、等待 −20%。
- **部署双锚**（同目录新旧二进制背靠背）：Qwen3-Embedding-8B Q8_0_ROCMFPX pp2048 **1114.9 → 1235.3 t/s（+10.8%）**；Ornith Q4_0_ROCMFP4_FAST tg128 71.99 → 71.95（持平）。

### 1.6 效果（llama-bench pp2048）

| 档位（Emb / Rnk） | 修复前 | 修复后 | 变化 |
|:--|:--|:--|:--|
| Q8_0_ROCMFPX | 1107 / 1099 | 1233 / 1227 | **+11.4% / +11.6%** |
| Q6_0_ROCMFPX | 920 / 916 | 1013 / 1010 | +10.1% / +10.2% |
| Q4_0_ROCMFP4_FAST | 1246 / 1244 | 1294 / 1298 | +3.9% / +4.3% |

### 1.7 复现命令

```bash
# 逐内核采样（两侧同条件各跑一次；采样开销约 5%，相对对比不受影响）
rocprofv3 --kernel-trace -f csv -o /tmp/prof -- \
  llama-bench -m <模型.gguf> -embd 1 -ngl 99 -p 2048 -n 0 -r 1

# 硬件计数器（注意 -- 分隔符；计数器名逐个传参；-f csv 必加）
rocprofv3 --pmc SQ_WAVES SQ_BUSY_CYCLES SQ_INSTS_SALU SQ_INSTS_BRANCH \
  SQ_WAIT_ANY SQ_WAVE_CYCLES --kernel-include-regex "mul_mat_q" \
  -f csv -o /tmp/pmc -- llama-bench -m <模型.gguf> -embd 1 -ngl 99 -p 2048 -n 0 -r 1

# 内核正确性
test-backend-ops test -o MUL_MAT -p "rocmfpx"   # 60/60
test-backend-ops test -o MUL_MAT -p "rocmfp4"   # 24/24
```

### 1.8 交付

- 提交：`0347fac53`（代码）、`f46544c29`（README 实测表刷新）
- 版本：**v2026.9.22 (v0.2.5)**（CHANGELOG 见仓库 `[v2026.9.22]` 条目）

---

## 2. 注意力内核：512 线程配置按 (ncols1,ncols2) 分裂（v1.0.0）

### 2.1 问题

gfx1151 的 FLASH_ATTN_EXT mma-f16 主内核在 stock 配置（`(256,256,64): 256,2,64` = 256 线程/occ2/combine64）下 VGPR 拉满 256、每线程溢出 **515 处**、scratch 1076B——kv≥8k 的深上下文吞吐被寄存器溢出拖累：kv=16k gqa8 基线 **11.48 TFLOPS**。

### 2.2 定位过程与证据

- 逐内核采样 + 硬件计数器：等待类 82%、L2 命中 6.8% → 延迟受限；空间维度指向寄存器压力。
- 单实例资源报告（`-Rpass-analysis=kernel-resource-usage`）：VGPR 256（上限）、溢出 515 处/线程、scratch 1076B。
- 512 线程配置单试（occ 2→1、combine 64→32）：溢出 **−60%**、kv=16k **+21%**（11.5→13.97）。
- 整行修改版（ncols=64 全实例共享）导致 `(32,2)` 家族 **−16~21%**——同宽度不同头分组共享同一行，必须按 `(ncols1,ncols2)` 分裂。

### 2.3 根因

配置表按 `ncols` 取键：`ncols=64` 一行同时服务 `(8,8)`/`(16,4)`/`(32,2)` 三种实例；512 线程收益只对头分组宽的 `(8,8)`/`(16,4)` 成立，对 `(32,2)` 相反。

### 2.4 修复

- `ggml/src/ggml-cuda/fattn-mma-f16.cuh`：新增分裂判定双 helper——设备侧 `ggml_cuda_fattn_mma_xaihi_split_512_dev`（宏判定）、主机侧 `ggml_cuda_fattn_mma_xaihi_split_512_host`（运行时 `amd_wmma_available(cc)`），接 5 处调用点（launch_bounds / kernel body nthreads / process_tile combine / host nthreads / host combine）；仅 `(256,256,8,8)` 与 `(256,256,16,4)` 启用。
- **关键坑（主机/设备宏可见性）**：`RDNA3` 宏源自设备趟专属 `__GFX11__`，主机编译趟不可见——单一 `__host__ __device__` 判定会形成「主机 256 / 设备 512」错位，表现为 **NaN（首元素）+ 约 2× 假快**（半量计算）。必须拆双 helper。

### 2.5 验证链

- 数值门：`test-backend-ops test -o FLASH_ATTN_EXT -p "hsk=256.*type_K=f16"` = **148/152**；4 挂均为 kv≥32k 的既有精度边缘（stock 同款，非本轮引入）。
- 定位矩阵：nb 梯度（2/4/8/16/32/64）与 n_kv_max 梯度（256/512/1024/2048）全过；初版 `(8,8)`-nb≥8 的 NaN 回归在修复后清零。
- 性能（同刻背靠背）：`(8,8)` kv=16k **11.48→14.08 TF（+22.6%）**；`(16,4)` **8.84→10.63（+20.2%）**；P1 `(32,2)` 持平；32k/64k 深水 **+4%**；4k 短上下文 −3~6%（权衡内）。
- 构建确定性：同源同工具链重编两次产物逐字节一致（md5 复核）。

### 2.6 复现命令

```bash
# 数值门
test-backend-ops test -o FLASH_ATTN_EXT -p "hsk=256.*type_K=f16"

# 性能（同刻背靠背；perf 用例由参数化循环生成：nh=8 × ratio∈{1,4,8} × kv∈{4096..65536} × nb∈{1,4096}）
test-backend-ops perf -o FLASH_ATTN_EXT -p "hsk=256,hsv=256,nh=8,nr23=\[8,1\],kv=16384,nb=4096"
test-backend-ops perf -o FLASH_ATTN_EXT -p "hsk=256,hsv=256,nh=8,nr23=\[4,1\],kv=16384,nb=4096"
test-backend-ops perf -o FLASH_ATTN_EXT -p "hsk=256,hsv=256,nh=4,nr23=\[6,1\],kv=16384,nb=512"
```

### 2.7 交付

- 提交：`43ea02cb3`（配置分裂）、`c57487a5b`（FA 向量闸门）、`6d4fc573a`（用例覆盖）、`0eb0adcf4`（llama-bench 类型）
- 版本：**v1.0.0 (v0.2.7)**

---

## 3. 预填 GEMM：ROCmFP4 的 MMQ K 分块接上 FP4 专用值（v1.0.1）

### 3.1 问题

Qwen3.8-27B 的预填 GEMM（`Q4_0_ROCMFP4_FAST`）在 27B 主投影形状上只有 **27.5 TFLOPS**，仅为同形状 f16（33.3）的 82%——4.25 bpw 的权重读得比 f16 少 3.7 倍，吞吐反而更低。对照 gufo 同款模型的公开记录（预填 ≈ llama.cpp 的 2×），差距不在注意力，在 GEMM 本身。

### 3.2 定位过程与证据

1. **立尺**（`tests/test-backend-ops perf -o MUL_MAT`，27B 投影形状）：Q4 = 27.47 / 25.75 / 25.23 TFLOPS（三个主形状），f16 = 33.27 / 24.31 / 27.73。
2. **分派面审查**（`ggml/src/ggml-cuda/mmq-config-rdna3-5.cuh`）：类型 101（`Q4_0_ROCMFP4_FAST`）的条目挂着 `MMQ_ITER_K=256`；而 `mmq.cuh:10` 里已有 FP4 专用的 `MMQ_ITER_K_FP4=512`，从未被 ROCmFP4 条目引用。
3. **变体实测**（每次重编 + 同刻测量）：

   | 变体 | 17408×2048×5120 | 判定 |
   |:--|--:|:--|
   | baseline（256） | 27.90 TF | 对照 |
   | **iter512（K=512）** | **53.33 TF（+90.6%）** | 采用 |
   | occ1（occupancy 2→1） | 27.34 TF | 否决（无收益） |
   | both（512 + occ1） | 52.57 TF | 否决（无叠加） |

### 3.3 根因

类型 101 接入 MMQ 时，配置表照抄了通用路径的 K 分块 256，FP4 专用值漏接——**纯配置遗漏，不是算法缺陷**。

### 3.4 修复

`ggml/src/ggml-cuda/mmq-config-rdna3-5.cuh`：`Q4_0_ROCMFP4` / `Q4_0_ROCMFP4_FAST` 的 12 条 CASE 条目将 `MMQ_ITER_K` 改为 `MMQ_ITER_K_FP4`。

### 3.5 验证链

- **数值门**：`MUL_MAT -p rocmfp4` **24/24**、`-p rocmfpx` **61/61**。
- **整网 A/B**（Qwen3.8-27B Q4_0_ROCMFP4_FAST，同机同模型同协议背靠背）：

  | 场景 | 改前 | 改后 | 变化 |
  |:--|--:|--:|--:|
  | pp2048 | 433.5 t/s | **720.8 t/s** | **+66.3%** |
  | pp16384 | 396.4 t/s | **613.5 t/s** | **+54.8%** |
  | pp65536 | 295.0 t/s | **413.6 t/s** | **+40.2%** |
  | tg256 | 14.04 t/s | 14.04 t/s | 不变（解码走 MMVQ） |

- **横向对照**：gufo 同款模型的公开记录为预填 656 / 575 / 387（0 / 16k / 64k 深度），本版为 721 / 614 / 414——三档均在其上（量化档不同，口径为同芯片同模型级别）。

### 3.6 边界与否决项

- **fp8 族不适用**：把同一改动套到 `Q2/Q3/Q6/Q8_0_ROCMFPX` → 内核 GEMM 20.2→34.8 TF（+72~86%），但数值门 **49/61**，失败用例全部为 **k=1024**（ERR≈1.0，结果整块错），k=256/k=4096 通过。K 瓦片与 SRAM 布局绑定（K 分位超块粒度 256），改动涉及块索引/加载器，属独立改造，未纳入本版。
- occ1 / both 无收益，不采用。

### 3.7 复现命令

```bash
# 内核用例（新增 27B 投影形状）
LD_LIBRARY_PATH=build/bin:/opt/rocm/core-10.1/lib \
  build/bin/test-backend-ops perf -o MUL_MAT -p "m=17408,n=2048,k=5120"
# 数值门
LD_LIBRARY_PATH=build/bin:/opt/rocm/core-10.1/lib build/bin/test-backend-ops test -o MUL_MAT -p "rocmfp4"
# 整网
build/bin/llama-bench -m <27B Q4_0_ROCMFP4_FAST.gguf> -p 2048 -n 0 -r 3
```

### 3.8 交付

- 代码：`mmq-config-rdna3-5.cuh`（12 条条目）+ `tests/test-backend-ops.cpp`（新增 perf 用例）。
- 数值门、端到端 A/B 数据在 `~/bench/gemm-e2e-1017/`、`~/bench/mmq-scan2-1017.log`。

## 4. SSM 卷积：去掉每层、每个 ubatch 的整块 concat（v1.0.1）

### 4.1 问题

预填阶段的内核采样（pp512，`rocprofv3`，内核总时长 1375 ms）里，`concat_cont` 一个内核占 **888.8 ms / 64.6%**，调用 793 次——比 GEMM、注意力、量化加起来还多。这不是计算，是纯拷贝。

### 4.2 定位过程与证据

1. **内核采样**（`rocprofv3` → sqlite → 按内核名聚合）：`concat_cont` 793 次 / 888.8 ms / 64.6%；`mul_mat_vec_q` 145.2 ms / 10.6%；`quantize` ~123 ms / 9.0%；`k_get_rows` 97.8 ms / 7.1%；`k_bin_bcast` 80.5 ms / 5.9%。
2. **调用点定位**：`src/models/delta-net-base.cpp` 的 `build_conv_state()` 每层 SSM、每个 ubatch 执行一次 `ggml_concat(ctx0, conv_states, qkv_mixed, 0)`。793 ≈ 50 个 SSM 层 × 8 个 ubatch × 2。
3. **对照 gufo 的做法**：卷积直接在「状态 + 新 token」的虚拟拼接上取窗口，不物化拼接结果。

### 4.3 根因

`ggml_ssm_conv` 只有一个输入，卷积窗口跨在「历史状态」和「本批 token」之间，所以调用方必须先把两者拼成一块连续内存。拼接本身不产生任何新信息，纯属算子接口的表达力不足。

### 4.4 修复

新增算子 `GGML_OP_SSM_CONV_STATE`：`conv_x`（本批 token）与 `conv_state`（前 d_conv-1 个 token）分开传入，卷积窗口按 `i2..i2+nc-1` 跨两个张量取值；旧 `GGML_OP_SSM_CONV` 单输入语义原样保留。

- API/CPU：`ggml/include/ggml.h`、`ggml/src/ggml.c`、`ggml/src/ggml-cpu/ops.cpp`
- CUDA：`ggml/src/ggml-cuda/ssm-conv.cu`（寄存器环与共享内存两条内核都按张量真实步长索引：通道走 `nb[1]`、行/序列分别走 `nb[0]` 与 `nb[2]`）
- 图：`src/models/delta-net-base.cpp` 不再 concat，状态回写视图补一次转置以匹配状态缓冲的元素序；`qwen3next` / `qwen35` / `qwen35moe` 三处下游不再重复卷积。

### 4.5 验证链

- **算子测试**：`test-backend-ops test -o SSM_CONV` **63/63**（45 条既有 + 18 条新增带 state 的用例，含 n_t=64 长序列与 n_s=3 多序列）。
- **端到端一致性**：同一提示（贪心、关闭思考）在改动前后**逐字一致**：「The capital of France is Paris. / The capital of Germany is Berlin. / The capital of Italy is Rome.」。
- **逐笔可编**：4 个提交各自 `cmake --build` 通过（含中途提交）。

### 4.6 效果（同机同模型同协议背靠背，r=3）

| 场景（Qwen3.8-27B，Q4_0_ROCMFP4_FAST） | concat 路径 | state 路径 | 变化 |
|:--|--:|--:|--:|
| 预填 pp512 | 704.50 t/s | **745.40 t/s** | **+5.8%** |
| 预填 pp2048 | 715.57 t/s | **750.57 t/s** | **+4.9%** |

> 内核占比 64.6% 是**时间占比**，而 concat 是带宽受限的纯拷贝，去掉后不会等额转化为整网收益——5% 左右是这条路径的实际汇率。

### 4.7 复现命令

```bash
# 算子测试（含新增的带 state 用例）
LD_LIBRARY_PATH=build/bin:/opt/rocm/core-10.1/lib \
  build/bin/test-backend-ops test -o SSM_CONV
# 整网 A/B
build/bin/llama-bench -m <27B Q4_0_ROCMFP4_FAST.gguf> -p 512,2048 -n 0 -r 3
```

### 4.8 交付

- 提交：`ggml: add SSM_CONV_STATE (API + CPU forward)`、`CUDA: support a separate conv state in SSM_CONV`、`tests: cover SSM_CONV with a separate conv state`、`models: convolve directly from the conv state, drop the per-layer concat`。

## 5. fp8 族的 K 分块：做到了正确，但撞上硬件天花板（v1.0.1）

### 5.1 问题

ROCmFP4 接上 512 的 K 分块后拿到 +90.6% 的内核吞吐，fp8 族（`Q8/Q6/Q3/Q2_0_ROCMFPX`）同样挂在 256 上，理应复制同一收益。但同族实测只有 19.99~25.33 TFLOPS，而 FP4 是 51.71——差距正好两倍。

### 5.2 定位过程与证据

1. **基准**（27B 主形状 `17408×2048×5120`）：q8 25.33、q2 23.91、q6 20.82、q3 19.99 TFLOPS；对照 FP4 51.71、f16 32.79。
2. **带宽核算**：q8 跑完该形状需 14.4 ms，权重仅 89 MB → 实际带宽 **6.2 GB/s**，而芯片上限 234 GB/s。**根本不是带宽受限。**
3. **结构审查**：fp8 的 loader、dp4a 尺寸宏、点积循环、内核主体的激活瓦片装载，四处都把 K 写死为 256。逐一参数化后，512 配置在数值上**跑通（`rocmfpx` 61/61）**。
4. **实测对比**（同机同刻）：

   | 配置 | 17408×2048×5120 | 5120×2048×17408 | 6144×2048×5120 |
   |:--|--:|--:|--:|
   | I=128，K=256（基线） | **25.33** | **23.36** | **24.45** |
   | I=96，K=512 | 23.47 | 21.29 | 23.82 |
   | I=64，K=512 | 23.09 | 18.07 | 22.92 |

5. **上限探针**：把内层的逐元素缩放（`C.x[l]*dA*dB`）整体去掉，只测矩阵核 → 25.67 TF，仅比基线快 1.3%。**缩放不是瓶颈。**
6. **硬件查证**（AMD GPUOpen，RDNA3 WMMA 数据）：每时钟每 CU 的 FLOPS —— FP16 512、BF16 512、**IU8 512**、**IU4 1024**。

### 5.3 根因

**这颗芯片的矩阵核对 4 位整数的吞吐是 8 位的两倍。** 实测 51.71 ÷ 25.33 = 2.04，与硬件 1024 ÷ 512 = 2 吻合。fp8 走 iu8 路径，25 TF 已经是该路径的天花板，不是代码缺陷。

附带的结构约束：8 位数据每个 K 占 1 字节（4 位只占 0.5），同样 64 KB 共享内存下，K=512 时 fp8 的 M 瓦片最多只能到 96 行（FP4 可以到 128）。M 瓦片的损失抵消了 K 翻倍的收益——这也是上表中 512 比 256 更慢的原因。

### 5.4 结论与否决项

- **否决**：fp8 族的 K=512 不采用。数值正确（61/61）但更慢，且瓶颈在硬件而非实现。
- **否决**：权重不进共享内存的重写（原计划 2–3 天）——上限探针显示内层缩放只占 1.3%，重写无法突破 iu8 的硬件上限。
- **保留**：`mmq_get_dp4a_tile_x_sizes` / `ggml_cuda_mmq_get_sram_stride` 的 K 参数化（提交 `a3eb67f67`），K=256 行为不变，作为后续任何 K 瓦片实验的基础设施。
- **附记**：`gfx1151` 的 WMMA 没有 FP8 输入形式（`v_wmma_*_fp8_fp8` 是 RDNA4/gfx1201 才加入的），RDNA3.5 的矩阵核只接受 f16/bf16/iu8/iu4。

### 5.5 复现命令

```bash
# fp8 基准（27B 投影形状）
LD_LIBRARY_PATH=build/bin:/opt/rocm/core-10.1/lib \
  build/bin/test-backend-ops perf -o MUL_MAT | grep q8_0_rocmfpx
# 数值门
LD_LIBRARY_PATH=build/bin:/opt/rocm/core-10.1/lib \
  build/bin/test-backend-ops test -o MUL_MAT -p rocmfpx
```

### 5.6 交付

- 代码：`a3eb67f67`（K 瓦片参数化，中性）、`a379d3950`（fp8 基准用例）。
- 否决记录：本节。

---

## 6. MTP 投机解码：把验证批次还给旧路径（v1.0.2）

**问题**。第 4 节的 conv-state 算子把卷积历史从输入里拆出去，前提是"这一批 token 自带完整的 `k-1` 窗口"。MTP 投机解码不满足这个前提：它的验证批只有 1~4 个 token，而且回滚快照要取到**批次之前**的状态行。开启 `--spec-type draft-mtp` 后表现为：能跑，草稿接受率 78.8%，但输出塌缩（` Paris the capital of France is Paris. Paris.`），与贪心基线相似度 0.015。

**定位过程**。先把 SSM 改造整体回退到引入前（`a67aaba31`）再跑同一组对照：MTP 输出与基线**逐字一致**，13.86 → 23.77 t/s。结论是改造破坏了投机路径，不是引擎本身的问题。随后两次改法都错：一是去掉回滚偏移里的 `-(k-1)`（坐标系换算项，去掉后预演直接崩），二是"短批次一律跳过回写"（解码状态不更新，生成退化）。真正的原因是回滚窗口会跨回批次之前的状态，新张量里没有那些行，怎么换算都补不出来。

**修法**。按批次分派，而不是在公式上找补：

| 场景 | 路径 |
|:--|:--|
| 纯预填（`n_rs_seq == 0` 且批次 ≥ `k-1`） | 新算子 `GGML_OP_SSM_CONV_STATE` |
| 单 token 解码、图预演（批次 < `k-1`） | 原 `ggml_concat` + `ggml_ssm_conv` |
| MTP 回滚（`n_rs_seq > 0`） | 原 `ggml_concat` + `ggml_ssm_conv` |

**结果**。同提示、同 seed、贪心：输出与基线**逐字一致**，解码 14.0 → 34.3 t/s（默认深度）；pp2048 保持 756.8 t/s，`SSM_CONV` 2/2 后端通过。

**调参**（服务端，同提示同 seed，逐档核对输出）：

| 草稿深度 `--spec-draft-n-max` | 生成 t/s | 相对基线 | 输出一致 |
|--:|--:|--:|:--|
| 关闭 | 14.0 | 1.00× | — |
| 3（默认） | 34.2 | 2.44× | ✓ |
| 4 | 34.8 | 2.48× | ✓ |
| 5 | 37.1 | 2.65× | ✓ |
| 6（推荐） | 37.9 | 2.71× | ✓ |
| 7 | 34.8 | 2.49× | ✓ |
| 8 | 8.4 | 0.60× | ✗ |
| 10 | 7.7 | 0.55× | ✗ |

**边界**。深度 ≥8 时草稿接受率跌到 **0.63%**（958 个草稿接受 6 个），只剩草稿开销，且输出不再与基线一致。这个 `nextn` 头只训练了很浅的步数，超出训练深度后草稿基本是噪声。推荐值定为 6；7 可用但更慢。

**为什么值得记一笔**。单流解码此前已到带宽上限（16.1 tok/s）的 87%，继续磨内核最多再挤 7%。投机解码换了方向：一次读权重、产出多个 token，因此能越过单流上限。这也是"解码先砍读字节"这条方法论在我们自己引擎上的第一次兑现。

## 7. 64K 解码：深度与 KV 类型都要按上下文分档（v1.0.2 之后）

**起点**。64K 单流解码 9.66 t/s，比 gufo 公开的 10.13 低约 4%。先怀疑两个方向：KV 流量、投机解码。

**KV 类型不是越省越快**。同一段 6 万 token：

| KV | 解码 t/s |
|:--|--:|
| f16 | 9.66 |
| q8_0 | 8.69 |
| q4_0 | 10.53 |

q8_0 反而更慢——反量化开销压过了省下的流量；只有 q4_0 拿到 +9%。这条否掉了"把 KV 降到 8 bit 就能提速"的直觉。

**投机深度不是越深越好**。同一段长文，生成 48 token：

| 深度 | 64K t/s | 短上下文（≤ 8k）t/s |
|--:|--:|--:|
| 关闭 | 9.9 | 14.0 |
| 3 | **17.5** | 34.2 |
| 4 | 14.0 | 34.8 |
| 5 | 13.2 | 37.1 |
| 6 | 12.4 | **37.9** |
| 8 | 5.4 | 8.4 |

短上下文的最优点在 6，长上下文在 3。原因不是草稿质量变差这么简单：长上下文下每轮验证都要读整段 KV，**深度的边际成本随上下文线性上升**，而命中率的边际收益在下降。

**一次误判，值得记下**。中途看到 64K 下投机输出与基线不同，我先判断成"回滚污染了 KV"。三个对照把它否掉了：

- 同一提示把上下文容量从 7 万压到 1.2 万，结果不变（不是缓存越界）；
- 关闭后端采样、草稿改贪心、`p_min=1`（几乎拒绝全部草稿），三者输出完全一样（不是采样状态）；
- 同一份维基文本只取 2k token，**同样会偏**（不是长上下文）；
- 而 64K + `The capital of France is` 这种确定续写，**逐字一致**。

结论：开投机时验证批一次算多个 token，矩阵归约顺序不同，浮点结果有极微小差异；在续写含糊处这一点差异会翻 token，并因自回归放大。**这是数值差异，不是状态损坏**。

**落地建议**。短上下文深度 6、长上下文深度 3；需要再快一档时叠加 q4_0 KV。不要把深度 8 当成"更激进所以更好"——实测接受率塌到 0.6%，只剩开销。

## 8. MTP 多序列：预填与解码不能混批（v1.0.4）

**问题**。单序列开投机正常（54%~59% 接受率），两个序列同时解码时跌到 2%~6%，四个及以上接近零；`--spec-draft-n-max 4` 起还会输出 ` - - - - ` 一类乱码。v1.0.3 曾把投机解码按槽串行来绕过，输出正确，但多用户吞吐受损。

**定位**。草稿侧先被排除：同一序列在单流与双流下的 `pending_h` 校验和、草稿首 token 完全相同，快照槽的数值也与单流逐位一致。真正错位的是**目标模型的 nextn h 行账本**：服务端把**正在预填的槽**（5 个 token）和**正在解码的槽**（1 个 token）合进同一个 `common_batch`，两条序列的 token 数不等。目标模型会把它拆成两个 ubatch，而 MTP 的 `process()` 按**整批位置**索引 h 行——第二个序列读到了第一个序列的 h 行，草稿因此变成对方的续写，验证与输出一起坏掉。

**修复**。`server_slot::can_batch_with()` 只允许**同一处理阶段**的槽合批（`state` 相同）。预填与解码不再混批；两个预填请求、两个解码请求仍可正常合批。v1.0.3 的按槽串行限制随之撤销。提交 `026becb9d`。

**验证**。

| 场景 | 接受率 | 速度 | 输出 |
|:--|--:|--:|:--|
| 单流 depth6（128 token） | 58.9% | 37.9 t/s | 正确 |
| 双序列 depth4 | 87.5% / 63.8% | 42.5 / 31.9 t/s | 正确 |
| 双序列 depth6 | 88.3% / 57.1% | 50.4 / 34.8 t/s | 正确 |
| 四路 depth2 | 75.5%~100% | 合计约 109 t/s | 正确 |
| 五路 depth1 | 80%~100% | 合计约 115 t/s | 正确 |

**边界**。这是根因修复，不是绕过：多序列投机解码恢复合批，老边界（验证批总行数超过 8 就崩）不再出现。预填与解码不混批——两者每条序列的 token 数不同，本来就不该进同一批；两个预填请求仍可合批。另外，`process()` 现在会校验每个序列的 token 在批内连续；不同序列的 token 数可以不等（槽位的草稿长度本来就可能不同），只有交错的批会被拒绝。

## 附：条目结构（后续内核级改动沿用）

1. 问题（含基线数字与适用条件）
2. 定位过程与证据（逐内核采样 / 硬件计数器 / 对照臂）
3. 根因（具体文件与函数）
4. 修复（提交号 + 代码要点）
5. 验证链（位等价 / 内核测试 / 计数器复核 / 部署双锚）
6. 效果数据表与复现命令
