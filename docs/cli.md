# 命令行

## 统一入口

```bash
strix <command> [options...]
```

| 命令 | 转发到 | 用途 |
|:--|:--|:--|
| `serve` | `strix-server` | OpenAI 兼容服务 |
| `run` | `llama-cli` | 交互式 / 单次生成 |
| `bench` | `llama-bench` | 基准测试 |
| `quantize` | `llama-quantize` | 量化 GGUF |
| `mtmd` | `llama-mtmd-cli` | 多模态命令行 |
| `diagnose` | —（本程序实现） | 检查 ROCm、GPU、二进制与模型 |
| `version` | —（本程序实现） | 打印本库版本与上游基线 |

`strix help` 打印这张表。

## 与上游二进制的关系

上游的二进制名一个都没改：`llama-cli`、`strix-server`、`llama-bench`、`llama-quantize`、`llama-mtmd-cli` 都还在原位置，直接调用完全有效。`strix` 只是分发器——它**不解析**子命令之后的参数，原样透传给目标程序，所以目标程序的 `--help`、`--version` 和所有选项行为不变。

```bash
strix bench --help      # 等价于 llama-bench --help
strix run --version     # 等价于 llama-cli --version
```

之所以这样做，是因为逐个把 `llama-*` 改名成 `strix-*` 会牵动 CMake target 名、Dockerfile 的 `COPY`、Windows 上的软链分支，而收益只是"名字统一"。统一入口拿到同样的效果，改动面却只有一棵新目录。

## 转发机制

`execv` 替换当前进程，不做 fork——因此信号、退出码、标准输入输出都原样传给调用者，`Ctrl-C` 的行为与直接运行目标程序一致。

查找顺序：先看 `strix` 自身所在目录（用 `/proc/self/exe` 定位），找不到再走 `PATH`。这样 `build/hip-gfx1151/bin/strix` 会优先用同目录的 `strix-server`，不会误用系统里另装的旧版本。

## `diagnose`

```bash
strix diagnose [-m model.gguf]
```

逐项输出：

- **版本**：`strix.cpp <version> (upstream base <sha>)`、`llama_build_info()` 的完整指纹、编译器与构建目标。
- **ROCm**：`ROCM_PATH`（默认 `/opt/rocm`）是否存在、`.info/version` 的版本号、`HSA_OVERRIDE_GFX_VERSION` 是否设置。
- **GPU**：`rocminfo` 里前几个 agent 与 `gfx*` 名称。`rocminfo` 不可用时明确报不可用，不猜。
- **同目录二进制**：五个目标程序逐个标 `ok` / `missing`。`missing` 通常意味着 preset 没把那个 target 编出来——例如 `cpu-check` 不编 `llama-mtmd-cli`。
- **模型**：给了 `-m` 时检查文件存在与 `GGUF` 魔数。

`diagnose` 只做只读探测，不加载模型、不初始化 HIP 上下文，因此可以在一台没装 ROCm 的机器上安全运行并给出"缺什么"。

## 退出码

- `0`：成功。
- `1`：未知命令、`diagnose` 完成但没有任何可报告项（目前不会发生）。
- `127`：目标程序既不在同目录也不在 `PATH` 上。
- 其他：目标程序自己的退出码，原样透传。

## 上游对应文档

- 服务端参数：`tools/server/README.md`
- CLI 参数：`tools/cli`（上游）、`tools/llama-bench/README.md`
- 本库运行示例：README 的「运行」一节
