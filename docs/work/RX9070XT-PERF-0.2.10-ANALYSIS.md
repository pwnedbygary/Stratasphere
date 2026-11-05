# RX 9070 XT performance: this branch's state, the 0.2.10 merge, and the fastest configurations

What was done on 2026-11-04/05 in this checkout, what it means for the user's PC, and what to run. Written by the
Lead from four independent workstreams; each has its own report:

- [UPSTREAM-0.2.10-PERF-TRIAGE.md](UPSTREAM-0.2.10-PERF-TRIAGE.md) - what upstream changed from 0.2.6 to 0.2.10 that
  helps this PC, with the measured numbers and their hardware.
- [MERGE-0.2.10-AUDIT.md](MERGE-0.2.10-AUDIT.md) - the semantic audit of the merge (fork features vs upstream files).
- [BUILD-TEST-0.2.10-HIP.md](BUILD-TEST-0.2.10-HIP.md) - the build, ctest, device self-test and hipBLASLt results on
  this PC, with the exact commands.
- [RX9070XT-32GB-PERF-CONFIGS.md](RX9070XT-32GB-PERF-CONFIGS.md) - the per-model/quant configuration decision
  document, memory math and citations.

**The PC:** AMD Radeon RX 9070 XT 16 GB (gfx1201; 16,304 MiB, desktop holds 1.1-2.0 GiB), Ryzen 7 7800X3D, 32 GB RAM
(30 GiB usable), ROCm 7.2.4, Linux.

**No throughput was measured in this session.** There are no model files on the PC (see "State before"), so the only
numbers below are quoted from the repository's own measured tables with their hardware, or arithmetic. The build,
the test suite, the device self-test and one hipBLASLt table were measured here and are reported as such.

---

## 1. The state before this session

The checkout was branch `codex/rx9070xt-perf-0.2.6` at `023834f1`: 33 commits of fork work on an upstream 0.2.6 base
(merge-base `4b9e05a2`; the base tree is upstream `v0.2.6`). The fork work was:

- **The dense Qwen3.8-27B engine** (Saluki, Swift) on the qwen35 dense path, opt-in through `STRATA_Q35_DENSE=1`:
  matrix-core prompt and decode attention on gfx12 Linux by default (`08688d57`), the IQ1_S/Q2_K decode kernels
  (`3718811a`), 1024-thread FFN-branch norms (`aa99d643`), sampled requests drafting with MTP (`e810fbbe`,
  `c8660a98`), and the fork's gfx1201 hipBLASLt tables for the dense prompt shapes (`71bda5e2`).
- **Conversation checkpoints** for the dense engine (`STRATA_Q35_CHECKPOINTS=1..64`, opt-in), about 149.6 MiB a
  snapshot on the 27B models; measured on this PC at ~118K: three edited/pruned requests started in 3.55-10.52 s
  instead of 148-155 s, whole run 4 min 11 s against 11 min 15 s ([CONVERSATION_CHECKPOINTS.md](../CONVERSATION_CHECKPOINTS.md)).
- **The opt-in native C++ server and model router** (`STRATA_NATIVE_SERVER=ON`, `frontend/`).
- **Tiel-Coder support settings** for the qwen36 engine (`STRATA_Q36_SKIP_MTP`, `_LEAN_ARENA`,
  `_EXPERT_RESERVE_MIB`; measured on this PC in commit `650737c9`).
- The Oct 25-26 measured profile tables for this PC in [MODEL_RESULTS.md](../MODEL_RESULTS.md).

The **installed app engine 0.1.56** (upstream update feed, `f-amd-vision c75ab7655503`) is a different product from
the fork build. Verified by string search in this session: it does not contain `STRATA_Q35_DENSE`,
`STRATA_Q35_CHECKPOINTS`, `STRATA_Q36_SKIP_MTP`, `STRATA_Q36_LEAN_ARENA` or `STRATA_Q36_EXPERT_RESERVE_MIB`, and
upstream 0.2.10's source has no dense Qwen35 model path. **The installed engine therefore cannot run the 27B dense
models at all, and cannot run Tiel-Coder's Q3_K MTP file**; those need the fork build. The installed engine also
bundles its own hipBLASLt 100300, so the gfx1201 100202 tables do not apply to it.

Also stale on the PC, unrelated to performance:

- `strata` is not on the PATH (the Terminal is not installed; only the app's engine is).
- `~/.strata/data/state.json` says setup is incomplete, cancelled in its model step; the model/pack folders the
  app's `native-tune/*.json` point at (`/home/garyb/Mounts/NVME/strata/...`) do not exist, and the `--expert-profile`
  paths point at a deleted AppImage mount (`/tmp/.mount_strataFKIomL/...`). No Qwen models are installed.

---

## 2. What this session changed

### 2.1 The upstream merge (branch `codex/rx9070xt-perf-0.2.10`)

`5eab60ce` merges upstream `github.com/Niko1221/Strata` main `ffa80d73` (0.2.10, engine 0.1.56) into the fork
branch: **1,441 upstream commits**, tags v0.2.7, v0.2.7.1, v0.2.8, v0.2.9 and v0.2.10, 1,426 files changed
(+169,493/-6,807).

Only **one file conflicted**, `CMakeLists.txt`, in two small hunks. Resolved as: project version 0.1.56 (upstream)
plus the fork's `STRATA_NATIVE_SERVER` option block; and both the fork's `if(STRATA_NATIVE_SERVER) add_subdirectory(frontend)`
block and upstream's `STRATA_BUILD_SPLIT` option/`add_subdirectory(src/split)` kept, each with its own `endif()`.
Everything else auto-merged; the nine files both sides touched were audited one by one (see the merge audit report).

The fork's features survive the merge. The audit ([MERGE-0.2.10-AUDIT.md](MERGE-0.2.10-AUDIT.md)) verdict is: **no
blocker and no silent loss of fork code**. The merged tree is byte-identical to git's own auto-merge for the eight
non-conflicted shared files; all 2,925 lines the fork added to the nine shared files are present; 60 of the fork's 69
changed files are byte-identical to the fork parent. The built binary contains all fork features (verified by string
search): `STRATA_Q35_DENSE`, `STRATA_Q35_DECODE`, `STRATA_Q35_CHECKPOINTS`, `STRATA_Q36_SKIP_MTP`,
`STRATA_Q36_LEAN_ARENA`, `STRATA_Q36_EXPERT_RESERVE_MIB`. The engine reports `strata 0.1.56`, the same engine
version the installed app runs, now with the fork's dense engine and the upstream 0.2.10 work in one tree.

Three **inherited upstream behaviour changes** (not merge damage) to know about before comparing against the Oct
tables, all off-switchable: the sampled-request penalty window changed (#4590; `STRATA_PENALTY_PROMPT=1` restores
the old window, so sampled dense answers can differ from the measured tables); the gfx12 `EXPDEC` decode default is
on for the qwen35 MoE expert kernels (`STRATA_EXPDEC=0` for an A/B); and the new warm-up `reset()` was checked not to
free the dense engine's preallocated checkpoint buffers (the preallocation runs first and the drop returns them to
the spare list, so `STRATA_Q35_CHECKPOINTS` behaves as measured).

### 2.2 What the merge brings for this PC

Ranked by expected value on a 16 GB gfx1201 card (full table with sources in
[UPSTREAM-0.2.10-PERF-TRIAGE.md](UPSTREAM-0.2.10-PERF-TRIAGE.md)):

| # | Change | Measured on | What it means here |
|---|---|---|---|
| 1 | Fused IQ2_S / IQ4_XS prompt experts now default on Linux gfx12 (`02c367d5`) | R9700, IQ3_S, 54% resident: prompt +10.8% at 2K, +12.2% at 4K, +13.2% at 8K, +10.0% at 32K (5/5 pairs); quick suite lost 2/19 tasks when it was opt-in | The Flash-Next prompt path (the pack's remaining MMQ layers join the fused int8 kernels). Needs a build with `-DSTRATA_PREFILL_MMQ=ON`. |
| 2 | CPU prefill share default on one gfx12 card (serve) (`d2068bde`) | R9700 IQ3_S: prompt +18.3% at 500, +30.6% at 1K; 2K/8K unchanged; KL 0.022 at 1K, top-1 29/29 | Short-prompt time-to-first-token on a card that streams experts. |
| 3 | PCIe decode miss split starts at 0.15 on gfx12 (`8d56d417`) | R9700: decode +9.9% at 8K (86.1 -> 94.6), +25.6% at 32K (67.2 -> 84.4), +10.4% for a short served chat; first-token KL 0.0013, same top-1 in 12/12 | More missing experts go to the idle CPU pool instead of PCIe. Most useful here, where most experts are missing. |
| 4 | gfx12 decode defaults (`3fd5a748`, `82eaeb42`, `09982fff`) and `STRATA_IL_PAD` (`4c5a5549`) | R9700 IQ3_S 8K: the four together +3.5% decode (3/3 pairs, answer md5 identical); HEAD_MIX_MULTI +0.8% on json_extract | Exact (or bit-stable) decode kernels for both the MoE and the dense path. |
| 5 | Matrix-core QSA block scorer above 28,672 tokens by default (`c31f1fa3`) | R9700, serve, 3 pairs: 32K prompt +1.83% (3/3), 98K +5.74% (3/3); first-token KL mean 0.0018, max 0.0055; needle 25/25 | Long-prompt reads above ~28K. Answers change there; below the floor it stays exact. |
| 6 | RDNA4 routed-expert, draft, shared-expert and head kernels (`1f8ca8de`, PRs 3259/3264) | no published number; the S2/head parts were run on gfx1201 only | Static expectation of a small decode/draft gain; unquantified. |
| 7 | DFlash2 drafter on HIP; gfx1201 a tested card (`29e8a47a`, `b353ce7d`, `d55138e9`) | gfx1201: +7.2% short, +1.35% long; one card, one request; needs a one-time pack (522 MB, 715-890 MiB VRAM against the MTP layer's 1,082) | Only for Flash-Next (not Coder, not the dense 27B). Auto is MTP; DFlash2 is opt-in in the run config. |
| 8 | Setup streams the KV cache from 64K up on a PC with room for experts + 10 GB (`ee455af7`) | 12 GB mimic, IQ3_S: 756 -> 1,267 expert slots; 32K prompt 21-34 s -> 12-17 s; 118K prompt 76-122 s -> 45-63 s | More expert slots, faster hybrid prompts -- but only for configs setup (re)writes on 0.2.10. |
| 9 | `fit_start` prices a discrete HIP card like CUDA (`076a195b`, `2e7db84b`) | R9700: base 3,900 MiB; walks `--kv-resident` 20,480/8,192/4,096 only | A safety net for a squeezed card; never changes context or request count. On 16 GB/128K it should leave a normal Flash-Next start alone. |
| 10 | HIP build takes 16-row verify/batch windows (`9d3c8c36`) | no one-card decode number; it is capacity for layer splits/batch groups | Not a single-request speed lever. Keep `--spec 3/4`. |

**Not for this PC:** the PLE table in RAM (26.8 GiB table + arena + 8 GiB cannot fit 30 GiB usable RAM; the auto
policy declines, direct SSD reads stay), the 12 GB-NVIDIA route prior, layer-split/pipelined-window work, and the
ARC/Mac changes. KVA ("faster long prompts", `STRATA_KVA=1`) is opt-in and a quality trade: R9700 27K needle prompt
+28% prompt but -11% decode, and answers can lose details.

---

## 3. Validation on this PC

Full detail and all commands are in [BUILD-TEST-0.2.10-HIP.md](BUILD-TEST-0.2.10-HIP.md); the short form:

| Step | Result |
|---|---|
| Configure | The command in `docs/AMD_HIP.md` fails on this ROCm tree: `The HIP compiler identification is unknown`, then `unsupported CUDA gpu architecture: gfx1201`. Root cause: `/opt/rocm/llvm` is a symlink under `/opt/rocm/lib`, so the compiler takes `/opt/rocm/lib` as the ROCm root and the ID link test cannot find `-lamdhip64`. Fixed with explicit `--rocm-path=/opt/rocm --rocm-device-lib-path=/opt/rocm/amdgcn/bitcode -L/opt/rocm/lib` in `CMAKE_HIP_FLAGS` (or `ROCM_PATH=/opt/rocm` in the environment). |
| Build | `strata` 120 s at `-j8` (peak 12.9 GiB); all targets +25 s; 199 executable link targets / 190 binaries; **0 compile errors**. No missing or changed upstream API in `src/qwen35`, `src/kernels`, `src/prefill`, `src/program/generate.cpp` or `frontend/`. |
| ctest | 197 tests: before the fix below 177 passed, 14 skipped, 6 failed (106.5 s); after it **178 passed, 14 skipped, 5 failed** (`ctest-after-isa-fix.log`). The 5 remaining failures are not code regressions: 3 need model fixtures (`ple_parity`, `expert_parity`, `pool_test`), 1 is a ROCm 7.2 runtime allocation expectation (`conversation_copy_streams_test`, 148 MiB vs the test's <16 MiB), 1 is a HIP K-quant MMQ gap only under the `STRATA_PREFILL_MMQ=ON` flag we added (`hip_prefill_mmq_parity`; its Q2_0 cases pass). |
| Device self-test | `strata-device --selftest` OK: gfx1201, wave32, memory plan fits. 39 GPU parity/kernel tests passed, including route, QSA, verify/spec, KV stream and the fused prompt-expert tests. |
| hipBLASLt gfx1201 table | **Validated**: `gfx1201-hipblaslt-100202.txt` accepted (24 rows, gfx1201, version 100202), `launches=4 fallbacks=0`, 4/4 parity cases PASS against hipBLASEx. The two dense tables load (header/arch/version accepted) but cannot be numerically validated without a dense model; their data-row counts are 9 and 67. |
| Model run / throughput | **Not tested**: no model files on the PC. Nothing was fabricated. |

One repository fix came out of the audit: `isa_guard_check` failed with two violations (`iq512_reference_gu_rows`,
`iq512x_rows` in `src/kernels/cpu/iq_avx512.cpp` missing from `WIDE_PUBLIC` in `tools/check_isa_guard.py`). This is
inherited from upstream 0.2.10 (the functions and the same allowlist are in the merge parent, added by #3416), not
from the merge resolution. Added them to the allowlist with the same reasoning as their siblings (they run only
behind `cpu_avx512_ok()` / the `x512_one_ok` probe); the check now reports `0 not allowed`, the tool's own unit tests
pass, and the full ctest re-run after the change is 178 passed / 14 skipped / 5 failed of 197.

---

## 4. The fastest configurations (what to run)

The full reasoning, memory math and citations are in [RX9070XT-32GB-PERF-CONFIGS.md](RX9070XT-32GB-PERF-CONFIGS.md).
The short answer, with the source build of this branch required for the dense engine and Tiel:

| Model | Engine / quant | Context | Measured on this PC (Oct 25-26 tables unless noted) |
|---|---|---|---|
| Qwen3.8-27B dense | Fork dense engine, **Saluki 27B IQ2-mix + MTP**, reserves 1,536/1,536 MiB, `--mtp 2 --prefill 2048`, long-context hipBLASLt table | 131,072 | 8K: 1,317 prompt / 70.8 decode; 128K: 762 / 45.7 |
| Qwen3.8-27B dense | Swift 1.5 **IQ3_XXS + MTP**, same settings, physical reserve 1,024 MiB | 131,072 | 8K: 1,280-1,314 / 66.8-69.1; 128K: 731-755 / 15.9-17.9 |
| Qwen3.8-27B dense | Swift 1.5 IQ3_S + MTP -- quality choice, short context only | 8K measured | 8K: 1,180-1,256 / 65.5-68.6; 128K: 740 / 11.7-12.7 |
| Qwen3.8-Flash-Next | **Coder IQ1_M**, installed app engine 0.1.56, int8 KV, `--resident-experts` | 131,072 | ~55 decode / ~530 fresh prompt (app log, real requests, not a benchmark) |
| Qwen3.8-Flash-Next | **Q2_0**, Flash engine, int8 KV + `--kv-resident 32768` + 11 GiB RAM tier + 3,584 MiB reserve | 131,072 | 8K: 452 / 16.5; 128K: 672 / 19.8 |
| Qwen3.8-Flash-Next | IQ3_XXS, same config (better quality, 84.5% vs 80.6% top-token vs Q8_0) | 131,072 | 8K: 363 / 11.6; 128K: 435 / 9.0. IQ3_S is not viable on 31 GiB RAM |
| Qwen3.6-35B-A3B | **Strata qwen36**, `STRATA_Q36_SKIP_MTP=1 STRATA_Q36_LEAN_ARENA=1 STRATA_Q36_EXPERT_RESERVE_MIB=1536` | 16,384 | 8K: 1,725 prompt / 47.69 decode; 32K prompt 1,135 / 26.60 (Oct 28, untimed) |
| Qwen3.6-35B-A3B | **llama.cpp Vulkan**, 26 CPU expert layers, q8_0 KV | 262,144 | 8K: 1,928-1,942 / 40.4-41.3; limit: 990 / 29.8-32.1 |

The ordered fastest path per model, and the exact engine command lines, are in
[section 2 and section 4 of the configs document](RX9070XT-32GB-PERF-CONFIGS.md#2-exact-launch-configurations).
On this PC the 16 GB card plus 31 GiB RAM is the wall: the 27B dense model is the only family whose whole working
set fits VRAM + RAM, which is why Saluki is the fastest model here.

Three configuration facts worth repeating:

1. **The dense 27B and Tiel need the source build of this branch.** The installed app engine cannot run them (no
   dense Qwen35 path; no Q3_K MTP handling settings).
2. **The 100202 hipBLASLt tables only apply to the source build** (system ROCm 7.2.4 = hipBLASLt 1.2.2). The
   installed app engine bundles 100300. For the source build, `STRATA_HIPBLASLT_TUNING=<abs path to
   tools/hip/gfx1201-hipblaslt-100202.txt>` is the prompt lever validated here; the dense tables are for the dense
   engine and are not numerically validated yet.
3. **`STRATA_Q36_SKIP_MTP=1` is still required for Tiel/Cyber-Tiel** after 0.2.10: their MTP block's experts are
   Q3_K, which the native expert kernels do not take (no upstream Q3_K expert format in 0.2.7-0.2.10). With it,
   there are no MTP drafts, which is why Strata wins only at short prompts and llama.cpp Vulkan still owns 32K-262K.

---

## 5. Open items and what to do next

1. **Re-install or re-fetch the models, then benchmark the merged build.** The PC has no Qwen models; the app's
   setup is incomplete (cancelled at packing) and its saved configs point at a deleted AppImage mount. Until that is
   redone, no post-merge number can exist. The expected effect of the merge on this PC is Flash-Next decode
   +10-25% and prompt +10-13% (R9700-measured defaults, rows 1-3 above; unverified on a 16 GB card), and the dense
   27B path unchanged or slightly faster (gfx12 decode defaults + RDNA4 kernels), with answers possibly changing at
   near-ties on the Flash-Next path.
2. **A/B the hipBLASLt table on the dense 27B and Flash-Next.** The gfx1201 expert table is validated (fallbacks
   0); its speed effect was measured on an R9700 (32K prompt 638 -> 1,177 tok/s with a full IQ3_XXS set) but the
   docs note a table calibrated for this card was within noise end to end, so measure before keeping it. The dense
   tables need a dense model first.
3. **DFlash2 vs MTP A/B on this PC.** gfx1201 is a tested card (+7.2% short, +1.35% long) and it uses ~0.2-0.37 GiB
   less VRAM than the MTP layer, but this PC has never run the pair. It is Flash-Next only.
4. **Optional long-prompt trade:** `STRATA_KVA=1` (Flash-Next only, one card) measured +11% at 14K and +18% at 30K on
   an RTX 5070, with a real quality risk; `--ple-io mmap` gave +11% at 8K on an R9700 against direct reads and is a
   cheap A/B for this SSD-bound prompt path.
5. **Build documentation:** the configure command in `docs/AMD_HIP.md` fails on a ROCm tree whose `/opt/rocm/llvm` is
   a symlink under `lib/` (this ROCm 7.2.4). The working command is in the build report; it is a candidate line for
   that page.
6. **Tiel's MTP block is the one real feature gap left** for this family on Strata: accepting Q3_K experts in the
   native expert/MTP kernels would let the qwen36 engine draft on Tiel/Cyber-Tiel and could move the 32K crossover.
   That is a kernel-format addition, not a configuration change; unmeasured and not attempted here.
