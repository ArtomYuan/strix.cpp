# Build presets and artefact layout

Generic upstream build options are in `docs/build.md`. This page covers the four project presets, where artefacts land, and the errors that actually happen.

## The four presets

| Preset | Purpose | Key variables |
|:--|:--|:--|
| `hip-gfx1151` | production build, the only supported runtime target | `GGML_HIP=ON`, `AMDGPU_TARGETS=gfx1151`, `GGML_OPENMP=OFF`, `CMAKE_INSTALL_RPATH=$ORIGIN/../lib` |
| `cpu-check` | compile check on machines without a GPU, and in CI | `GGML_NATIVE=OFF`, `GGML_CCACHE=OFF` |
| `sanitizer` | debugging the CPU path with ASan/UBSan | `GGML_SANITIZE_ADDRESS=ON`, `GGML_SANITIZE_UNDEFINED=ON`, `LLAMA_BUILD_TESTS=ON` |
| `base` | hidden, inherited by the three above | `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, `LLAMA_CURL=OFF` |

```bash
cmake --preset hip-gfx1151
cmake --build --preset hip-gfx1151
```

Each `buildPresets` entry shares its name with a configure preset and carries the target list, so local and CI builds compile the same targets.

## Artefact layout

```
build/<preset>/bin/     # executables and shared libraries
build/<preset>/         # CMake cache, compile_commands.json
```

For example `build/hip-gfx1151/bin/strix-server`. To install elsewhere, `cmake --install build/hip-gfx1151 --prefix <dir>` produces a `bin/` + `lib/` layout with `$ORIGIN/../lib` in the RUNPATH, so the tree can be moved freely.

## Prerequisites

- ROCm 10.0.0 at `/opt/rocm` by default; elsewhere, override with `-DCMAKE_PREFIX_PATH=<path>`.
- CMake 3.21+ (preset file version 3; without presets upstream needs 3.14).
- gcc / g++, taken from `/usr/bin/gcc` and `/usr/bin/g++` by default and overridable.

## What a CPU build proves, and what it does not

`cpu-check` compiles `ggml-base`, `ggml-cpu`, `llama`, `common` and every tool, including the CPU reference implementations in `rocmfp4.c`/`rocmfpx.c`. It catches skeleton-level breakage, type-table misalignment and broken includes.

It does **not** verify HIP kernels: with `GGML_HIP=OFF`, `ggml-cuda` is not entered at all. Any commit that touches a kernel must run `hip-gfx1151` and `test-backend-ops` again on gfx1151.

## Upstream examples assume a different path

Upstream `examples/*/README.md` and `tests/fusion/README.md` write `./build/bin/...`, the layout of an upstream default build. With this project's presets the path is `build/<preset>/bin/...`. Those upstream documents are left untouched — rewriting them would conflict on every sync.

## Common errors

- **`cmake --preset` reports an unknown preset**: CMake older than 3.21. Upgrade, or fall back to explicit `-D` flags.
- **configure cannot find HIP**: ROCm is not under `/opt/rocm` and `CMAKE_PREFIX_PATH` was not passed.
- **Binaries cannot find their `.so` after unpacking**: the tree was not produced by `cmake --install`, or the install RPATH is missing; `hip-gfx1151` already sets `$ORIGIN/../lib`.
- **CI compiles more than a local build**: CI uses `cpu-check`, whose target list includes `strix`, `llama-cli`, `llama-quantize`, `llama-bench` and `strix-server`.
