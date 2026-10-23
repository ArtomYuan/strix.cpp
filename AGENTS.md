# Instructions for strix.cpp

strix.cpp is a fork of llama.cpp maintained by ArtomYuan. The upstream `ggml-org/llama.cpp` agent policy does not apply here; use this repository's rules.

## Ground rules

- Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing code.
- AI-assisted work is allowed when the human contributor understands the change, can debug it, and can maintain it.
- Disclose meaningful AI assistance in the pull request.
- Do not open automated or mass-generated PRs without human review.
- Build and test locally on gfx1151; this repository runs only the two minimal workflows in `.github/workflows/` (`build`, `docs`). The upstream workflow set is not restored.

## Scope

- The primary server binary is `strix-server`; `llama-server` is a compatibility symlink.
- Keep ROCmFPX / ROCmFP4 / mtmd behavior intact when syncing upstream.
