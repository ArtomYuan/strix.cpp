# 构建 preset 与产物布局

上游的通用构建选项见 `docs/build.md`。本篇只讲本库的四个 preset、产物路径和常见错误。

## 四个 preset

| preset | 用途 | 关键变量 |
|:--|:--|:--|
| `hip-gfx1151` | 生产构建，唯一支持的运行目标 | `GGML_HIP=ON`、`AMDGPU_TARGETS=gfx1151`、`GGML_OPENMP=OFF`、`CMAKE_INSTALL_RPATH=$ORIGIN/../lib` |
| `cpu-check` | 无 GPU 机器与 CI 的编译检查 | `GGML_NATIVE=OFF`、`GGML_CCACHE=OFF` |
| `sanitizer` | 调试 CPU 路径，带 ASan/UBSan | `GGML_SANITIZE_ADDRESS=ON`、`GGML_SANITIZE_UNDEFINED=ON`、`LLAMA_BUILD_TESTS=ON` |
| `base` | 隐藏，供上面三个继承 | `CMAKE_EXPORT_COMPILE_COMMANDS=ON`、`LLAMA_CURL=OFF` |

```bash
cmake --preset hip-gfx1151
cmake --build --preset hip-gfx1151
```

`buildPresets` 与 `configurePresets` 同名，构建目标写在 preset 里，所以本地和 CI 编的是同一批 target。

## 产物布局

```
build/<preset>/bin/     # 可执行文件与共享库
build/<preset>/         # CMake 缓存、compile_commands.json
```

例如 `build/hip-gfx1151/bin/strix-server`。安装到别处用 `cmake --install build/hip-gfx1151 --prefix <dir>`，会按 `bin/` + `lib/` 布局落盘，二进制带 `$ORIGIN/../lib` 的 RUNPATH，可以在整包内自由移动。

## 前置

- ROCm 10.0.0，默认 `/opt/rocm`；装在别处则 `-DCMAKE_PREFIX_PATH=<path>` 覆盖。
- CMake 3.21+（preset 文件版本 3 的要求；不用 preset 时上游最低是 3.14）。
- gcc / g++，默认取 `/usr/bin/gcc` 与 `/usr/bin/g++`，可在命令行覆盖。

## CPU 构建能验证什么、不能验证什么

`cpu-check` 会编译 `ggml-base`、`ggml-cpu`、`llama`、`common` 与全部工具，包括 `rocmfp4.c`/`rocmfpx.c` 的 CPU 参考实现。它能抓住骨架级破坏、类型表错位、include 断链。

它**不能**验证 HIP kernel：`ggml-cuda` 在 `GGML_HIP=OFF` 时根本不进构建。任何改到 kernel 的提交，必须在 gfx1151 上另跑一次 `hip-gfx1151` 与 `test-backend-ops`。

## 上游示例的路径假设

上游的 `examples/*/README.md`、`tests/fusion/README.md` 里写的是 `./build/bin/...`，那是上游默认构建目录的布局。用本库 preset 时对应 `build/<preset>/bin/...`。这些上游文档保持原样，不改写——否则每次同步都会冲突。

## 常见错误

- **`cmake --preset` 报 unknown preset**：CMake 低于 3.21。升级，或退回手写 `-D` 参数。
- **configure 找不到 HIP**：ROCm 不在 `/opt/rocm`，且没有传 `CMAKE_PREFIX_PATH`。
- **解包后二进制找不到 `.so`**：没走 `cmake --install` 或 install RPATH 没设；`hip-gfx1151` 已带 `$ORIGIN/../lib`。
- **CI 编得比本地多**：CI 用 `cpu-check`，target 列表包含 `strix`、`llama-cli`、`llama-quantize`、`llama-bench`、`strix-server`。
