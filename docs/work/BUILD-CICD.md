# Stratasphere builds (CI/CD)

`.github/workflows/stratasphere-build.yml` builds this fork on GitHub-hosted Ubuntu runners with AMD's ROCm 7.2.4
packages (no GPU is needed: the HIP kernels are compiled for `gfx1200;gfx1201`, not run).

| Trigger | What happens |
|---|---|
| push to `main`, `codex/rx9070xt-perf-0.2.10`, `codex/rx9070xt-32gb-profile` (every merge) | builds `strata`, `strata-dsv41`, `strata-native` and the router AppImage; uploads them as a workflow artifact (14 days) |
| nightly `0 4 * * *` UTC | the same build of the default branch |
| push of a `v*` tag | the same build, then creates the tag's GitHub release if needed and uploads the assets (`--clobber`) |
| manual (`workflow_dispatch`) | the same build |

Artifacts: `Stratasphere-linux-x86_64.AppImage` (router; bundles a pinned llama.cpp Vulkan CLI plus its libraries and
needs the host's ROCm at `/opt/rocm`), `stratasphere-linux-x86_64.tar.gz` (`strata`, `strata-dsv41`,
`strata-native`) and `SHA256SUMS`.

The AppImage tool (`tools/build_native_appimage.py`) takes `--vulkan-libs DIR` so the llama.cpp release tarball's
`*.so*` files travel beside `llama-vulkan` (their RUNPATH is `$ORIGIN`); without it the binary would not start.
