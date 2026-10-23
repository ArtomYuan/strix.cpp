# 测试

## 四层

| 层 | 位置 | 跑什么 | 什么时候跑 |
|:--|:--|:--|:--|
| 算子 | `tests/test-backend-ops.cpp`（上游，类型表已扩） | 全类型 `MUL_MAT`，含 ROCmFP 族 | 改 kernel / 改类型表 |
| 量化 | `tests/test-spec-batch-contiguity.cpp`（自有）、`llama-quantize` 往返 | 量化与反量化、批连续性 | 改量化代码 |
| Parity | `tests/parity/` | 与上游同量化对比 | 发版前 |
| 单元 | `tests/*.cpp`（上游） | 各组件 | 改对应模块 |

## 怎么跑

```bash
# 算子：三组基线
build/hip-gfx1151/bin/test-backend-ops -p rocmfp4 -o MUL_MAT
build/hip-gfx1151/bin/test-backend-ops -p rocmfpx -o MUL_MAT
build/hip-gfx1151/bin/test-backend-ops -o MUL_MAT

# 量化往返
build/hip-gfx1151/bin/llama-quantize --allow-requantize \
  <源.gguf> <输出.gguf> Q4_0_ROCMFP4_FAST 32

# 批连续性（自有）
build/hip-gfx1151/bin/test-spec-batch-contiguity

# Parity（需要上游对照构建，见 tests/parity/README.md）
tests/parity/run.sh --model <模型.gguf> \
  --strix build/hip-gfx1151/bin/llama-bench \
  --upstream build/upstream/bin/llama-bench
```

`test-backend-ops` 的三个过滤参数：`-o` 算子名、`-b` 后端名、`-p` 参数（类型名）过滤。所以 `-p rocmfp4` 是"只要名字里带 rocmfp4 的用例"。

## 验收基线（2026-10）

| 项 | 通过线 |
|:--|:--|
| `MUL_MAT -p rocmfp4` | 24/24 |
| `MUL_MAT -p rocmfpx` | 61/61 |
| 默认 `MUL_MAT` | 2534/2534（ROCm + CPU 双后端） |
| `MUL_MAT_ID` | 953/953（MoE 路由，重复专家修复后） |

数字随版本变化时，以 `docs/upstream-sync.md` 与 `CHANGELOG.md` 上一版记录为准；这里的表是基线快照。

## CI 现状

| workflow | 跑在哪 | 覆盖 |
|:--|:--|:--|
| `build` | GitHub 托管 runner | CPU 编译 + `strix version` 双断言 |
| `docs` | GitHub 托管 runner | 全部 markdown 相对链接 |
| `hip` / `parity` / `release` | 自建 gfx1151 runner（模板） | 见 `docs/ci/README.md` |

托管 runner 没有 ROCm，也没有 gfx1151，所以 kernel 与 parity 只能靠自建 runner 或本地。

## 已知缺口

- **本地无 ROCm 时只验证得了 CPU 路径**。`cpu-check` 不进入 `ggml-cuda`，改了 kernel 却只跑 CPU 构建，等于没测。
- **Parity 要独立构建上游**，耗时且占空间；不放进每次 push 的 CI，只在发版前或每周跑。
- **没有数值黄金样本（golden files）**。当前用 `test-backend-ops` 的相对误差判据，不做逐 bit 对比；引入 golden 需要固定模型与固定随机种子，尚未做。
- **量化质量只有零星 PPL 记录**（见 `CHANGELOG.md`），没有系统化的 PPL 基线表。
