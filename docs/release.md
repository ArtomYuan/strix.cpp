# Release process

strix.cpp follows semantic versioning for its own releases: `vMAJOR.MINOR.PATCH`. GitHub Release names and CHANGELOG headings use `vX.Y.Z (YYYY-MM-DD)`; git tags contain only the version.

`LLAMA_VERSION_*` in the root `CMakeLists.txt` is the upstream llama.cpp sync version. It is **not** the strix.cpp release tag; do not bump it for a strix.cpp release.

## Before tagging

- Add a `CHANGELOG.md` / `CHANGELOG.en.md` section with the version, date, changes, and verification results.
- Build and smoke-test on gfx1151 (see [CONTRIBUTING.md](CONTRIBUTING.md)).
- Update README/docs/model cards when commands, binary names, or version labels change.

## Cutting the release

1. Commit the release notes with `chore(release): vX.Y.Z`.
2. Create an annotated tag: `git tag -a vX.Y.Z -m "vX.Y.Z"`.
3. Push `master` and the tag. This repository has no GitHub Actions workflows; maintainers push with a token-authorized remote.
4. Create a GitHub Release titled `vX.Y.Z (YYYY-MM-DD)` with bilingual notes.

The upstream `scripts/release.sh` and the `make-release` workflow are **not used** by this fork; they refer to upstream automation that is not present here.

## Compatibility

- `strix-server` is the primary server binary. Builds and installs also provide `llama-server` as a compatibility symlink.
- The 2026-09 releases are tagged `v0.2.0`…`v0.2.6`. The upstream llama.cpp `v0.2.0` tag is preserved as `upstream/v0.2.0`.
