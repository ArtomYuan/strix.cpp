# CI that needs a real gfx1151

`.github/workflows/` 里只有 `build.yml` 与 `docs.yml` 两个能在 GitHub 托管 runner 上跑的 workflow。下面三个需要 ROCm 与 gfx1151，托管 runner 没有，所以它们以 `.example` 的形式放在这里——**不要直接改名放进 `.github/workflows/`**，否则每个 push 都会排队等一个不存在的 runner。

## 启用方式

1. 在一台 gfx1151 机器上注册 self-hosted runner，label 至少包含 `self-hosted` 与 `gfx1151`。
2. runner 上装好 ROCm（默认 `/opt/rocm`）、`cmake`、`git`、`gh`。
3. 把需要的文件去掉 `.example` 后缀，移进 `.github/workflows/`。
4. `parity.yml` 需要仓库变量 `PARITY_MODEL`（指向一个本地 GGUF 的绝对路径），否则跳过。

## 三个文件

| 文件 | 触发 | 做什么 | 通过标准 |
|:--|:--|:--|:--|
| `hip.yml` | push 到 master（源码路径）/ 手动 | gfx1151 全量构建 + 三组 `test-backend-ops` | rocmfp4 24/24、rocmfpx 61/61、默认 MUL_MAT 2534/2534 |
| `parity.yml` | 每周一 04:00 UTC / 手动 | 本库与 `upstream.txt` 同 sha 的上游各跑一遍 bench，出比值表 | 同量化 pp 行无超噪声回归 |
| `release.yml` | 打 `v*` tag / 手动 | 构建、打包、生成 SHA256、上传到 Release | 资产可见且校验和匹配 |

数字会随版本变化，以 `docs/upstream-sync.md` 的验收栏为准；这里的表格给的是 2026-10 基线。
