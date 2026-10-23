# Command line

## Single entry point

```bash
strix <command> [options...]
```

| Command | Forwards to | Purpose |
|:--|:--|:--|
| `serve` | `strix-server` | OpenAI-compatible server |
| `run` | `llama-cli` | interactive / one-shot generation |
| `bench` | `llama-bench` | benchmarking |
| `quantize` | `llama-quantize` | quantize a GGUF |
| `mtmd` | `llama-mtmd-cli` | multimodal CLI |
| `diagnose` | — (implemented here) | check ROCm, GPU, binaries, model |
| `version` | — (implemented here) | print release version and upstream base |

`strix help` prints this table.

## Relationship to the upstream binaries

No upstream binary was renamed: `llama-cli`, `strix-server`, `llama-bench`, `llama-quantize` and `llama-mtmd-cli` all remain in place and can be called directly. `strix` is only a dispatcher — it does **not** parse the arguments after the command name, it passes them through unchanged, so `--help`, `--version` and every option behave exactly as before.

```bash
strix bench --help      # same as llama-bench --help
strix run --version     # same as llama-cli --version
```

The alternative — renaming each `llama-*` binary to `strix-*` — would drag in CMake target names, Dockerfile `COPY` lines and the Windows symlink branch, for the sole benefit of a consistent name. A single entry point achieves the same with one new directory.

## How forwarding works

`execv` replaces the current process; there is no fork. Signals, exit codes and standard streams therefore pass straight through, and `Ctrl-C` behaves exactly as if the target had been started directly.

Lookup order: first the directory containing `strix` itself (located via `/proc/self/exe`), then `PATH`. So `build/hip-gfx1151/bin/strix` prefers the sibling `strix-server` and will not silently pick up an older system-wide install.

## `diagnose`

```bash
strix diagnose [-m model.gguf]
```

What it prints:

- **Version**: `strix.cpp <version> (upstream base <sha>)`, the full `llama_build_info()` fingerprint, compiler and build target.
- **ROCm**: whether `ROCM_PATH` (default `/opt/rocm`) exists, the version from `.info/version`, and whether `HSA_OVERRIDE_GFX_VERSION` is set.
- **GPU**: the first agents and `gfx*` names from `rocminfo`. If `rocminfo` is unavailable it says so instead of guessing.
- **Sibling binaries**: the five tool binaries marked `ok` / `missing`. `missing` usually means the preset did not build that target — `cpu-check` does not build `llama-mtmd-cli`, for example.
- **Model**: with `-m`, file existence and the `GGUF` magic.

`diagnose` is read-only: it does not load a model and does not initialise a HIP context, so it is safe to run on a machine without ROCm and will report what is missing.

## Exit codes

- `0`: success.
- `1`: unknown command.
- `127`: target binary found neither beside `strix` nor on `PATH`.
- Anything else: the target program's own exit code, unchanged.

## Upstream documents

- Server options: `tools/server/README.md`
- CLI options: `tools/cli` (upstream), `tools/llama-bench/README.md`
- Run examples for this project: the "Run" section of the README
