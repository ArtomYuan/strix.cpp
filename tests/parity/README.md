# Parity benchmarks

这套管线要证明两件事，分开看，不能混。

1. **同量化不退化**：`Q4_0`、`Q4_K_M`、`Q8_0` 这些上游也支持的类型，strix 引擎不应比同基线的 llama.cpp 慢。这是判断 kernel 改动有没有污染标准路径的判据——比“我们更快”重要。
2. **同 BPW 跨格式**：strix 的 4.50 BPW `Q4_0_ROCMFP4` 对上游 4.5 BPW `Q4_0`，必须同时看速度与 PPL。只报速度是误导。

## 前置

- 两份构建：本库的 `llama-bench`，以及按 `upstream.txt` 的 sha 独立构建的上游 `llama-bench`。
- 同机、同 ROCm、同 gfx1151、同一份模型文件、同 ctx、同 reps。
- 记录时钟、温度、电源模式。脚本只测吞吐，不测环境；环境不记，数据就不可复现。

## 用法

```bash
# 1. 上游对照构建（只需一次；只拉一个 commit）
git init build/upstream-src
git -C build/upstream-src remote add origin https://github.com/ggml-org/llama.cpp
git -C build/upstream-src fetch --depth 1 origin "$(cat upstream.txt)"
git -C build/upstream-src checkout FETCH_HEAD
cmake -S build/upstream-src -B build/upstream -DGGML_HIP=ON \
  -DAMDGPU_TARGETS=gfx1151 -DLLAMA_CURL=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/upstream -j "$(nproc)" --target llama-bench

# 2. 两侧同形状跑一遍并出表
tests/parity/run.sh \
  --model /path/to/model.gguf \
  --strix build/hip-gfx1151/bin/llama-bench \
  --upstream build/upstream/bin/llama-bench

# 3. 只要原始数据或 JSON 输出
tests/parity/run.sh ... --out build/parity
tests/parity/compare.py build/parity/strix.jsonl build/parity/upstream.jsonl --json
```

`run.sh` 把两侧的 `-o jsonl` 分别写进 `strix.jsonl` 与 `upstream.jsonl`，然后调
`compare.py` 配对。原始数据保留下来，便于事后重算而不必重跑。

## cases.json

`pp` / `tg` / `reps` 只定义形状，模型与量化由命令行给。改 `cases.json` 等于同时改两侧——不允许只改一边。

## 判读

- `ratio` > 1 表示 strix 侧更快。
- 同量化的 `pp` 行若出现 `ratio` < 1 且超出噪声带（约 3-5%），视为回归：先查 kernel 改动是否波及了标准路径，而不是先怀疑测量。
- 跨格式行必须与 `tests/quant` 的 PPL 一起看，单独一行速度没有意义。
