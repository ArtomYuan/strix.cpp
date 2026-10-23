# 上游同步批次记录

本页记录每一次从 `ggml-org/llama.cpp` 合并的**批次**，而不是每次提交。CHANGELOG 记「这次同步改了什么」，本页记「这次同步从哪到哪、跳过什么、用什么验证」——两者互补，不重复。

同步的操作流程、冲突分级与回滚约定见 [CONTRIBUTING.md 的「上游同步」](../CONTRIBUTING.md#上游同步)。

## 当前基线

| 项 | 值 |
|:--|:--|
| 上游基线 commit | `091b6a7fc` |
| 基线日期 | 2026-10-14 |
| 上次 merge commit | `b5a7b6118` |
| 本库发布版本 | 见 `CMakeLists.txt` 的 `STRIX_VERSION` |
| 构建可读版本 | `strix-server --version` 的 `upstream base` 一行 |

## 批次表

| 日期 | 上游窗口 | commits | merge commit | 冲突面 | 验证 |
|:--|:--|--:|:--|:--|:--|
| 2026-10-14 | `f072b10371..091b6a7fc` | 552 | `b5a7b6118` | 1176 files；7 处真内容冲突 | rocmfp4 24/24、rocmfpx 61/61、默认 MUL_MAT 2534/2534 |
| 2026-09-20 | `97e4ca735..f072b10371` | 98 | `c305d4a9a` | — | rocmfpx 61/61 |
| 2026-09-14 | `e107984bc..97e4ca735` | 172 | `cfeb42ff6` | 24 个机械冲突 + 4 个真冲突 | 预/后留证 tag `sync-20260914-pre` / `sync-20260914-post` |
| 2026-09-03 | `—` → `e107984bc` | 117 | `e1697de49` | — | — |
| 2026-08-28 | 初始 fork（v0.1.0） | — | — | — | — |

「—」表示当时的记录没有留下该项，不为它补造数字。

## 每次同步必须记什么

按顺序填，缺项就是没做完：

1. **上游窗口**：`<from-sha>..<to-sha>`，用 `git log --oneline <from>..<to> | wc -l` 取 commit 数，`git diff --stat <from>..<to> | tail -1` 取文件数。
2. **merge commit**：`git log --merges -1 --format=%h`。
3. **冲突面分类**：机械冲突（`.github/workflows/*` 的 delete/modify 裁决）与真内容冲突（逐段适配的文件清单）分开记，两者成本不同。
4. **跳过项与理由**：哪些上游改动**故意不合并**（例如 hexagon / ui / 其它后端在本库不构建），以及哪些上游新增的 workflow 被一并删除。
5. **验证结果**：`test-backend-ops -p rocmfp4`、`-p rocmfpx`、默认 `MUL_MAT` 三组数字，加 gfx1151 全量构建结论。

## 跳过的部分

这些不需要每次重新判断，直接沿用以节省时间：

- **CI workflows**：上游 `.github/workflows/*` 全部保持删除态；合并窗口里新增的 workflow 一并删除。本库只保留 `build.yml` 与 `docs.yml`。
- **非 ROCm 后端**：`ggml-hexagon`、`ggml-opencl`、`ggml-vulkan`、`ggml-sycl`、`ggml-musa` 等随上游合入源码，但不构建、不验证；冲突处理见 CONTRIBUTING 的三档分级。
- **`tools/ui`**：随上游合入，本库不构建 UI。
- **自有资产**：`ggml/rocmfp4/`、`ggml/rocmfpx/`、`template-instances/` 里 13 个自有实例，以及 `ggml-cuda` 下 30 个被改写的上游文件。完整清单与处理原则见 [CONTRIBUTING 的「自有资产清单」](../CONTRIBUTING.md#自有资产清单)。

## 回滚

merge 之前打 `pre-sync-YYYYMMDD` tag；同步在 `sync/upstream-YYYYMMDD` 分支上做，不夹带功能改动。回滚即 `git reset --hard pre-sync-YYYYMMDD`。
