# 基准

基准回答三个不同的问题，混在一起就会得出错误结论。

## 三类问题

1. **同量化对比**——`Q4_0`、`Q4_K_M`、`Q8_0` 这些上游也支持的类型，strix 引擎比同基线的 llama.cpp 快多少。这测的是引擎实现，也是"kernel 改动有没有污染标准路径"的判据。
2. **同 BPW 跨格式**——strix 的 4.50 BPW `Q4_0_ROCMFP4` 对上游 4.5 BPW `Q4_0`。这类必须同时报速度和 PPL；只报速度是误导，因为量化质量不同。
3. **单侧档位选择**——`Q4_0_ROCMFP4` / `_FAST` / `_COHERENT` / `_STRIX` 之间怎么选。不需要上游参与，跑本库一侧即可。

## 工具

`tests/parity/` 负责第 1、2 类：

- `cases.json` 定义形状（pp / tg / reps），两侧共用一份。
- `run.sh` 用同一份形状跑两个 `llama-bench`，各输出 JSONL。
- `compare.py` 按 `(test, n_prompt, n_gen)` 配对，输出比值表；`--json` 给机器读。

用法与上游对照构建的步骤见 `tests/parity/README.md`。

`llama-bench` 自身支持 `-o csv|json|jsonl|md|sql` 五种输出，所以两侧结果天然同构，可以进同一张表，也可以进同一个 SQLite 再查。

## 已有数据在哪

- **每版的实测数字**：`CHANGELOG.md` 对应版本条目。例如 2026-10 那次同步记录了 `Q4_0_ROCMFP4_FAST` 在 Qwen3.8-27B 上 pp256 418→742（+78%）、pp2048 399→762（+91%），tg256 持平。
- **每项内核特调的证据与复现**：`docs/development/kernel-optimization.md`，包括反向案例（哪些优化没通过数值门而撤回）。
- **历史对照数据**：`benches/`（上游原装，含其它平台的数字，不可与本机直接比较）。

本页不复述这些数字——它们会随版本变，抄一遍就有两份会过期的数据。

## 判读规则

- `ratio` > 1 表示 strix 侧更快。
- **同量化的 pp 行出现 `ratio` < 1 且超出 3-5% 噪声带，先查回归，不要先怀疑测量。** 这是这套工具存在的意义。
- 跨格式行必须与 PPL 一起看，并且注明两边各自的 BPW。
- `tg` 行受采样与 KV 类型影响更大，波动带比 `pp` 宽，单独比较意义有限。

## 公平性约束

同机、同 ROCm、同 gfx1151、同一份模型文件、同 ctx、`reps` ≥ 5 取中位数；记录时钟、温度、电源模式。两侧 `--version` 都要留档——本库会打出 `upstream base`，正好证明对照组没跑错 commit。

## 上游对应文档

- 参数与输出格式：`tools/llama-bench/README.md`
- 多份结果汇总：`scripts/compare-llama-bench.py`
- 跨 commit 对比：`scripts/compare-commits.sh`
