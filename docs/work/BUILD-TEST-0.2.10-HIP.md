# Build and test report: HIP engine on an RX 9070 XT (gfx1201), merged 0.2.10

Build and test report for the merged 0.2.10 branch, run on 2026-11-04 23:39 to 2026-11-05 00:00
(America/New_York). The engine was built out of tree for gfx1201, and its registered test suite, its
device self-test and the hipBLASLt tuning tables were run.

No model files are installed on this PC (no pack, no GGUF, no `pack/full/experts.bin`), so there is no
generation, no prompt or decode time and no throughput number in this report. Every number below was
measured on this PC in this run.

## Result

| Step | Result |
| --- | --- |
| Configure (documented command) | OK, but `The HIP compiler identification is unknown`; the build then fails on the first device source |
| Configure (working command, see below) | OK, 1.9 s, `The HIP compiler identification is Clang 22.0.0` |
| `strata` target, `-j8` | OK, 120 s, no compile error |
| All targets (`ninja all`, `-j8`) | OK, 25 s more, 199 executable link targets, 190 binaries, no compile error |
| `ctest -j4` | 197 tests: 177 passed, 14 skipped, 6 failed; 106.5 s |
| GPU device self-test | OK (`strata-device --selftest`, gfx1201 wave32) |
| hipBLASLt expert/sparse tuning table | Accepted, `launches=4 fallbacks=0`, 4 of 4 parity cases PASS |
| Model run, serving, throughput | Not tested: no model files on this PC |

The branch compiles. Every failure is either a missing test fixture, a hardware/feature gate, or an
inherited upstream test gap; none is a compile error and none is a fork-vs-upstream API break.

## What was built

| Item | Value |
| --- | --- |
| Worktree | `/home/garyb/LLM-Projects/Strata` |
| Branch | `codex/rx9070xt-perf-0.2.10` |
| HEAD | `5eab60ceb5f905234f3ad624d649df53eecc0cd6` "Merge upstream 0.2.10 (Niko1221/Strata main ffa80d73) into the RX 9070 XT performance branch" |
| Engine version | `strata 0.1.56` (from `strata --version` and `-DSTRATA_VERSION="0.1.56"` in the build) |
| llama.cpp / ggml source | pinned `3cf03257f219afbe7334045ff7c6a06ac68c627d` (`third_party/ggml/VERSION.txt`), ggml 0.24.0 |
| Build dir | `/home/garyb/Mounts/NVME/.agent-work/build-hip-0210-fixed` (892 MB, kept) |
| First-attempt build dir | `/home/garyb/Mounts/NVME/.agent-work/build-hip-0210` (evidence, kept) |

No repository source was edited. The only file written in the repo is this report.

## PC and toolchain

| Item | Value |
| --- | --- |
| GPU | AMD Radeon RX 9070 XT, `gfx1201`, wave32, 15.922 GiB VRAM (17,095,983,104 B), 32 compute units |
| CPU / RAM | Ryzen 7 7800X3D (16 threads), 30 GiB |
| Kernel | 6.18.55-2-cachyos-lts |
| ROCm | 7.2.4 in `/opt/rocm`; HIP runtime/driver version 70253211; hipBLAS 3.2; hipBLASLt 1.2.2 (version id 100202) |
| HIP compiler | `/opt/rocm/llvm/bin/clang++`, AMD clang 22.0.0git |
| Host compiler | gcc 16.2.1, cmake 4.4.5, ninja 1.13.2 |
| GPU access | user `garyb` is in `render` and `video`; `/dev/kfd` and `/dev/dri/renderD128` usable |

## Configure

### Attempt 1: the documented command (first failure)

The command from `docs/AMD_HIP.md` "Build":

```sh
cmake -S /home/garyb/LLM-Projects/Strata -B /home/garyb/Mounts/NVME/.agent-work/build-hip-0210 \
  -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSTRATA_ENABLE_HIP=ON -DSTRATA_ENABLE_CUDA=OFF \
  -DCMAKE_HIP_ARCHITECTURES=gfx1201
```

Configure finished ("Configuring done (13.9 s)") but with the line that matters:

```text
-- The HIP compiler identification is unknown
```

The build then stopped at step 10 of 349 after 3 s, on the first device source:

```text
[3/349] Building HIP object CMakeFiles/strata_core.dir/src/core/device.cu.o
FAILED: [code=1] CMakeFiles/strata_core.dir/src/core/device.cu.o
/opt/rocm/llvm/bin/clang++  -DSTRATA_HAVE_DFLASH2=1 -DSTRATA_HIP_ARCHS=\"gfx1201\" -DSTRATA_Q6K_EXPERTS=1 -DSTRATA_USE_HIP=1 -DSTRATA_VERIFY_MAX_T=16 -DSTRATA_VERSION=\"0.1.56\" -DUSE_PROF_API=1 -D__HIP_PLATFORM_AMD__=1 -I/home/garyb/LLM-Projects/Strata/include -I/home/garyb/LLM-Projects/Strata/include/strata/hip_compat -I/opt/rocm/include --offload-arch=gfx1201 -Wall -Wextra -include /home/garyb/LLM-Projects/Strata/include/strata/hip_compat/cuda_runtime.h -o CMakeFiles/strata_core.dir/src/core/device.cu.o  -c /home/garyb/LLM-Projects/Strata/src/core/device.cu
clang++: error: unsupported CUDA gpu architecture: gfx1201
```

The command has no `-x hip`, so clang compiled the `.cu` file as CUDA. That follows from the unknown
compiler id: CMake's Clang-HIP rules (`Modules/Compiler/Clang-HIP.cmake`, which is what adds `-x hip`)
are selected by the compiler id.

Root cause, measured in `build-hip-0210/CMakeFiles/CMakeConfigureLog.yaml`: CMake's HIP compiler-id
link test failed because clang could not find the HIP runtime library:

```text
Compiling the HIP compiler identification source file "CMakeHIPCompilerId.hip" failed.
Compiler: /opt/rocm/llvm/bin/clang++
Build flags:
Id flags:
The output was:
1
ld.lld: error: unable to find library -lamdhip64
clang++: error: linker command failed with exit code 1 (use -v to see invocation)
```

The same log shows the driver's diagnosis and its bad library path:

```text
Found HIP installation: /opt/rocm/lib, version 7.2.53211
... -L/opt/rocm/lib/lib -lamdhip64 ...
```

`libamdhip64.so` is in `/opt/rocm/lib`, not `/opt/rocm/lib/lib`. On this ROCm tree
`/opt/rocm/llvm` is a symlink to `/opt/rocm/lib/llvm`, so the real compiler binary is
`/opt/rocm/lib/llvm/bin/clang-22`; without `--rocm-path` (or `ROCM_PATH` in the environment) the
driver walks up two directories and takes `/opt/rocm/lib` as the ROCm root, then uses the wrong
include and library paths.

This is an environment/toolchain problem, not Strata code. Two minimal probes in
`/home/garyb/Mounts/NVME/.agent-work/probe-min` confirmed it:

- `ROCM_PATH=/opt/rocm` in the environment on a fresh minimal project: `The HIP compiler
  identification is Clang 22.0.0` (probe `b5`).
- `-DCMAKE_HIP_COMPILER_ROCM_ROOT=/opt/rocm` alone, without `ROCM_PATH` or `--rocm-path`: still
  `unknown` (probe `b4`).

### Attempt 2: the working command

A fresh build dir with the ROCm path made explicit (this is the command used for everything below):

```sh
cmake -G Ninja -S /home/garyb/LLM-Projects/Strata \
  -B /home/garyb/Mounts/NVME/.agent-work/build-hip-0210-fixed \
  -DCMAKE_BUILD_TYPE=Release \
  -DSTRATA_ENABLE_HIP=ON -DSTRATA_ENABLE_CUDA=OFF \
  -DSTRATA_BUILD_TESTS=ON -DSTRATA_PREFILL_MMQ=ON \
  -DCMAKE_HIP_ARCHITECTURES=gfx1201 \
  -DCMAKE_HIP_COMPILER=/opt/rocm/llvm/bin/clang++ \
  -DCMAKE_HIP_COMPILER_ROCM_ROOT=/opt/rocm \
  -DCMAKE_PREFIX_PATH=/opt/rocm \
  "-DCMAKE_HIP_FLAGS=--rocm-path=/opt/rocm --rocm-device-lib-path=/opt/rocm/amdgcn/bitcode -L/opt/rocm/lib" \
  -DSTRATA_GGML_DIR=/home/garyb/Mounts/NVME/.agent-work/build-hip-0210/_deps/strata_llamacpp-src
```

Key output:

```text
-- The HIP compiler identification is Clang 22.0.0
-- Strata: HIP enabled, arch gfx1201
-- strata: pack/full/experts.bin absent - the expert arena load test is NOT registered
-- trellis MMQ (Gyro prompts): overlay on the pinned ggml-cuda in .../ggml-tq-overlay
-- Configuring done (1.9s)
-- Generating done (0.1s)
-- Build files have been written to: /home/garyb/Mounts/NVME/.agent-work/build-hip-0210-fixed
```

Configure wall time 2 s; `STRATA_GGML_DIR` points at the llama.cpp checkout attempt 1 had already
fetched, so nothing was fetched again. The generated HIP rule in `CMakeFiles/rules.ninja` was checked
and does contain `-x hip`:

```text
command = .../opt/rocm/llvm/bin/clang++ $DEFINES $INCLUDES $FLAGS -MD -MT $out -MF $DEP_FILE -o $out -x hip -c $in
```

Configure registered 197 ctest tests and 199 executable link targets. `STRATA_BUILD_TESTS=ON` and
`STRATA_PREFILL_MMQ=ON` were added because the Lead's working command includes them; the stock
documented configure uses the documented defaults (`STRATA_BUILD_TESTS` on in a full source tree here
because `tests/` exists, `STRATA_PREFILL_MMQ` off).

The two scripts used are kept at `/home/garyb/Mounts/NVME/.agent-work/scripts/configure-fixed.sh` and
`.../build-target.sh`.

## Build

Built with `ninja -j8` in two stages, so the main engine's time is visible on its own:

| Command | Target | Result | Time | Peak system memory |
| --- | --- | --- | --- | --- |
| `ninja -j8 strata` | the engine | EXIT=0, 165 build edges | 120 s | 12,945 MiB used |
| `ninja -j8` | everything else (tests, benches, `strata-device`, ...) | EXIT=0, 387 edges cumulative | 25 s | 11,550 MiB used |

Total build time about 2 min 25 s from an empty build dir (configure excluded). `-j16` was not tried:
`-j8` already peaked at 12.9 GiB of the 30 GiB on this PC and the task said to watch for OOM. There was
no OOM and no compile error.

Warnings (not errors): 3,202 warning lines in the `strata` log and 982 in the all-targets log; 2,009
unique warning lines across both. By kind: `-Wunused-result` 1,690, `-Wsign-compare` 1,526,
`-Wunused-value` 776, `-Wunused-parameter` 80, `-Wpass-failed=transform-warning` 25, unused function
24, unused variable 17, unused private field 12, unused const variable 12. Most-affected files:
`src/kernels/cuda/native_mmvq.cu` (1,318), `src/kernels/cuda/fused_gr.cu` (354),
`src/prefill/prefill.cpp` (296), `src/kernels/cuda/iq_kernels.cu` (218),
`src/program/generate.cpp` (123). `src/qwen35/` has 8 warnings. The only deprecation warning is the
rocBLAS beta API in `tools/hip/tune_rocblas.cpp` (a tool, not the engine).

None of these are merge-integration problems as far as compilation can show: every file the fork and
upstream both touch (including `src/program/generate.cpp`, `src/prefill/`, `src/kernels/`,
`src/core/`, `src/qwen35/`) compiled against the merged tree, and no warning names a missing or changed
upstream symbol. The `hipError_t` "nodiscard" warnings are ROCm API annotations, not upstream-vs-fork
drift.

## ctest

Command (from the build dir; `LD_LIBRARY_PATH` only for the runtime libraries):

```sh
env LD_LIBRARY_PATH=/opt/rocm/lib ctest --test-dir . --output-on-failure -j4 --timeout 120
```

Result: 197 tests registered, 177 passed, 14 skipped, 6 failed, 106.5 s wall. ctest exit code 8. No
test timed out and nothing crashed.

### The 6 failed tests and their first error

1. `ple_parity` (missing model file):

```text
ple_parity: required PLE table is missing or incompatible: ../../Q2_0/Qwen3.8-Flash-Next-GSQ-RCO-Q2_0-00002-of-00002.gguf (cannot open ../../Q2_0/Qwen3.8-Flash-Next-GSQ-RCO-Q2_0-00002-of-00002.gguf)
```

2. `conversation_copy_streams_test` (device-memory expectation on this ROCm):

```text
FAIL: a card's first copy stream and event take under 16 MiB
conversation_copy_streams_test: the first copy stream + event of the card took 151552 KiB of device memory
```

3. `hip_prefill_mmq_parity` (feature gap under `STRATA_PREFILL_MMQ=ON` on HIP; the Q2_0 cases pass):

```text
synthetic-Q2_0-GU-pass0: type=42 rows=7 experts=3 ref_rms=1.98041 rel_l2=0.00106754 max_abs/ref_rms=0.00380488 zero_row_max=0
...
synthetic-Q4_K skipped: not built (STRATA_MMQ_KQUANTS off?)
synthetic-Q5_K skipped: not built (STRATA_MMQ_KQUANTS off?)
synthetic-Q6_K skipped: not built (STRATA_MMQ_KQUANTS off?)
synthetic-Q5_1 skipped: not built (STRATA_MMQ_KQUANTS off?)
MMQ parity screen failed: host quantizer unavailable
```

4. `expert_parity` (missing model pack artifact):

```text
cannot read expert 0 (layer 0) from pack/full/experts.bin
```

5. `pool_test` (same missing artifact):

```text
cannot read expert 0 of layer 0 from pack/full/experts.bin
```

6. `isa_guard_check` (CPU ISA guard report on the built library; see Findings):

```text
check_isa_guard: /home/garyb/Mounts/NVME/.agent-work/build-hip-0210-fixed/libstrata_kernels_cpu.a: 777 functions, 135 hold wide-ISA instructions, 2 not allowed
  VIOLATION strata::kernels::cpu::iq512_reference_gu_rows(int, unsigned char const*, unsigned long, unsigned long, int, void const* const*, int, float* const*, int, int) (169 wide instructions, first vxorps): wide-ISA code in an unlisted public function [iq_avx512.cpp.o]
  VIOLATION strata::kernels::cpu::iq512x_rows(int, unsigned char const*, unsigned long, int, void const* const*, int, float* const*, int, int) (685 wide instructions, first vpxord): wide-ISA code in an unlisted public function [iq_avx512.cpp.o]
```

### The 14 skipped tests, by class

ctest reported "The following tests did not run". Re-run individually with `-V` to read the skip
line; the messages are quoted:

- Windows-only tests: `io_priority_test` ("SKIP (Windows only)"), `pinned_alias_test` ("SKIP (Windows
  HIP only)").
- Hardware/feature gates on this card:
  - `hip_pf_cover_test`: "SKIP: no gfx11 WMMA device" (gfx1201 is gfx12).
  - `hip_prefill_wmma_gemm_parity`: "WMMA GEMM parity SKIPPED: gfx1201 has no WMMA matrix cores".
  - `hip_prefill_fused_moe`: "the fused kernels need sm_80 or newer: skipped".
  - `expert_cache_segmented_test`: "--vram-elastic (a segmented expert cache) is CUDA-only for now".
- Opt-in tuning table not set:
  - `hip_prefill_rocblas_gemm`: "SKIP: set STRATA_ROCBLAS_TUNING to a tuning table (or the tools/hip
    directory) for this GPU and rocBLAS build" (there is no gfx1201 rocBLAS table in `tools/hip/`).
  - `hip_prefill_hcd_exact_parity`: "SKIP: STRATA_HIPBLASLT_TUNING is not set"; with the table set it
    still skips with "SKIP: gfx1201 is not a gfx11 card".
  - `hip_prefill_hipblaslt_gemm`: skipped because `STRATA_HIPBLASLT_TUNING` was unset; it passes
    when the table is supplied (next section).
- No fork llama.cpp (trellis types) in this build: `native_hadamard_test`, `tq_cpu_parity`,
  `tq_cpu_parity_avx2` ("this ggml has no trellis types: skipped").
- Missing fixture directory: `iq_parity_fixtures` and `iq_parity` ("no fixtures in
  .../iq_fixture - skipped").

### GPU and CPU tests that ran and passed

GPU kernel/parity tests that ran on the card: `hip_q2_zero`, `hip_gemm_f16_io_parity`,
`hip_qsa_nr_parity`, `hip_expert_cache_staging`, `hip_intrinsics`, `hip_handoff`, `hip_mapped_alias`,
`hip_native_qsa_score`, `hip_prefill_native_batch`, `hip_prefill_gemm`, `hip_gdn_rec_head`,
`hip_gdn_chunked`, `hip_gdn_chunked_off`, `hip_gdn_chunked_default`, `hip_router_fast`,
`hip_prompt_attn_wmma`, `hip_device_selftest`, `qsa_scores_gfx1100_parity`, `qsa_parity`,
`qsa_topk_parity`, `gr_parity`, `gdn_parity`, `gdn_parity_split`, `kv_stream_parity`,
`kv_window_parity`, `kv_steps_parity`, `verify_parity`, `verify_batch_parity`, `spec_verify_parity`,
`spec_proposal_parity`, `sampler_parity*`, `rope_parity`, `quantize_act_parity`, `dequant_s2_parity`,
`s2_gemv_parity`, `s2_gemv_q8_parity`, `elementwise_parity`, `bf16_gemv_parity`,
`hip_prefill_fused_iq`, `hip_prefill_fused_iq_f16`. The two `hip_prefill_fused_iq` runs were the
longest at 77.8 s and 77.0 s; `kv_stream_parity` took 14.9 s.

Dense/native CPU-lane tests that passed: `dense_lowbit_test`, `dense_repack_test`,
`native_dense_ple_key_test`, `mmvf_rows_parity`, `native_grouped_parity`, `native_multi_parity`,
`native_arithmetic_parity` and its 9 variants (`_avx`, `_kq256`, `_mt1`, `_no512`, `_ggml`,
`_nvfp4_avx2`, `_nvfp4_ggml`, ...), `qwen35_sampling_test`, `qwen35_checkpoints_test`.

## Direct runs for the follow-up questions

### Window and limits unit tests (run directly, all exit 0)

These are CPU-side simulation/host tests; the engine binary itself was compiled with
`-DSTRATA_VERIFY_MAX_T=16` (seen in the generated compile flags).

| Binary | Result |
| --- | --- |
| `window_limits_test` | `window_limits_test: 11 checks passed (window 16 rows)` |
| `window_rule_test` | exit 0 |
| `window_evict_test` | `window_evict_test: 8 checks passed` |
| `penalty_window_test` | `penalty_window_test: 18 checks passed` |
| `host_limits_test` | `host_limits_test: ok` |
| `prefill_plan_test` | exit 0 |
| `stage_reserve_test` | `stage_reserve_test: 864 cases, PASS` |
| `routed_stream_test` | exit 0 |
| `qwen35_checkpoints_test` | `qwen35_checkpoints_test: ok` |

Two requested names do not exist here:

- `native_memory_policy`: it is `frontend/memory_policy_test.cpp`, registered only with
  `-DSTRATA_NATIVE_SERVER=ON` (`frontend/CMakeLists.txt:58-60`), and that option is off by default.
  It was not built.
- `hip_window*`: no test by that name exists in the tree. The window tests above are the ones that
  exist and all pass.

### hipBLASLt tuning table on this card (the prompt lever)

System hipBLASLt is 1.2.2, table version id 100202. The binary
`build-hip-0210-fixed/hip_prefill_hipblaslt_gemm` loads the table for the device architecture and
hipBLASLt version, then checks two rows it uses (bf16 N=48 K=2560 ldy=96 and f16 N=512 K=2560 ldy=512)
and compares the output with hipBLASEx.

Command (table = the expert/sparse table):

```sh
STRATA_HIPBLASLT_TUNING=/home/garyb/LLM-Projects/Strata/tools/hip/gfx1201-hipblaslt-100202.txt \
STRATA_HIPBLASLT_VERBOSE=1 ./hip_prefill_hipblaslt_gemm
```

Result: exit 0. The table was accepted and hipBLASLt ran it; there were no fallbacks:

```text
prefill gemm: hipBLASLt tuning enabled (24 rows, gfx1201, version 100202)
prefill gemm: Lt solution=73754 dtype=bf16 T=4096 N=48 K=2560 ldy=96 beta=0 workspace=0
prefill gemm: Lt solution=73754 dtype=bf16 T=37 N=48 K=2560 ldy=96 beta=1 workspace=0
prefill gemm: Lt solution=88193 dtype=f16 T=4096 N=512 K=2560 ldy=512 beta=0 workspace=0
prefill gemm: Lt solution=88193 dtype=f16 T=37 N=512 K=2560 ldy=512 beta=1 workspace=0
prefill gemm: hipBLASLt summary launches=4 fallbacks=0 unique_fallback_shapes=0
case dtype=bf16 T=4096 N=48 K=2560 ldy=96 output_offset=0 beta=0.0 relative_l2=4.05635129e-07 max_abs=4.76837158e-06 PASS
case dtype=bf16 T=37 N=48 K=2560 ldy=96 output_offset=48 beta=1.0 relative_l2=3.90878901e-07 max_abs=2.86102295e-06 PASS
case dtype=f16 T=4096 N=512 K=2560 ldy=512 output_offset=0 beta=0.0 relative_l2=2.57355965e-06 max_abs=2.47955322e-05 PASS
case dtype=f16 T=37 N=512 K=2560 ldy=512 output_offset=7 beta=1.0 relative_l2=2.38178127e-06 max_abs=1.57356262e-05 PASS
smoke test: 4 cases on 2 of the table's 24 rows; outputs match hipBLASEx (solution ids are not verified; see STRATA_HIPBLASLT_VERBOSE)
```

So the expert/sparse table is usable on this PC: the table is accepted for gfx1201 + hipBLASLt 100202,
hipBLASLt runs the table's solutions, `fallbacks=0`, and the output matches hipBLASEx on the rows the
smoke test covers. The smoke test does not verify the other 22 rows and does not verify that a given
solution id is optimal; it verifies correctness and that no fallback happened.

With the same binary, the two dense-engine tables:

```sh
STRATA_HIPBLASLT_TUNING=.../gfx1201-qwen35-dense-hipblaslt-100202.txt STRATA_HIPBLASLT_VERBOSE=1 ./hip_prefill_hipblaslt_gemm
STRATA_HIPBLASLT_TUNING=.../gfx1201-qwen35-long-context-hipblaslt-100202.txt STRATA_HIPBLASLT_VERBOSE=1 ./hip_prefill_hipblaslt_gemm
```

Both exit 1 with the same refusal:

```text
tuning table lacks the rows this smoke test uses (bf16 N=48 K=2560 ldy=96, f16 N=512 K=2560 ldy=512)
```

The table was not rejected (no "tuning table rejected: ..." line), so its header, architecture and
hipBLASLt version are accepted; it simply holds dense Qwen3.5 shapes, not the two sparse-expert rows.
Those tables cannot be numerically validated with this smoke test, and doing it needs the fork dense
engine and a dense model, which is not installed. Measured data-row counts: expert/sparse table 24
rows (the binary reports "24 rows", so the count agrees), dense table 9 rows, long-context table 67
rows. The untracked branch note `docs/work/RX9070XT-32GB-PERF-CONFIGS.md` says 11 and 69 rows; the
files have 9 and 67 data rows, so that note is off by two (static observation, low risk).

Without a table the binary exits 77 with:

```text
SKIP: set STRATA_HIPBLASLT_TUNING to a tuning table for this GPU and hipBLASLt version
```

That is also why ctest skips `hip_prefill_hipblaslt_gemm`.

## Smoke outputs

```text
$ ./strata --version
strata 0.1.56
```

```text
$ ./strata --help        (exit 0, 286 lines)
strata generate: opened 2 GPU queues in a fixed order (STRATA_HIP_QUEUE_PRIME=0 turns it off)
strata generate --pack DIR --tokens "1,2,3" [options]
  --pack DIR           the pack directory (default pack/full)
  ...
```

The first help line is worth noting: the engine opened its two HIP queues, so the HIP runtime works
from the built binary before any model is loaded.

```text
$ ./strata-device --list-devices
device 0: AMD Radeon RX 9070 XT
  arch gfx1201, 15.9 GiB, wave32
```

```text
$ ./strata-device --selftest
device 0: AMD Radeon RX 9070 XT
  HIP arch            gfx1201 wave32 (compiled for gfx1201)
  multiprocessors     32
  VRAM total / free   15.922 GiB (17095983104 B) / 14.924 GiB (16024338432 B)
  driver / runtime    70253211 / 70253211

memory plan  --max-context 20480
  KV + indexer keys   0.267387 GB
  recurrent state     0.000000 GB  (GDN recurrence + conv history, NOT evictable)
  expert cache        5.675613 GB  (4105 slots of 1,382,400 B)
  VRAM pool           5.943000 GB
  expert arena (DRAM) 0.000000 GB
  card free           14.924 GiB (16024338432 B)
  plan + KV vs free   FITS

selftest: arena 0.062 GiB (67108864 B), used 0.002 GiB (2097152 B) after two 1 MiB allocations
selftest: over-allocation refused as required
strata-device selftest OK
```

## What could not be tested, and why

- No model run, no prompt/decode speed, no throughput. This PC has no model files: there is no
  `pack/full/experts.bin` (the configure output says so), no `pack/` model pack, and no Q2_0/PLE GGUF
  (`~/.strata` holds 732 KB of settings, no model). Any speed number here would be invented, so none
  is given.
- No serving (Python server, OpenAI/Anthropic API, MCP, vision) and no end-to-end generation.
- The dense-engine tables (`qwen35-dense`, `qwen35-long-context`) are not numerically validated: the
  available smoke test covers sparse-expert shapes only, and the dense engine needs a dense model.
- The fork dense-engine path (`STRATA_Q35_DENSE=1` and the tuning knobs) was not run: no dense GGUF.
- Windows-only tests skipped (`io_priority_test`, `pinned_alias_test`); CUDA-only feature
  (`--vram-elastic`) skipped; gfx11-only kernels skipped; sm_80+ fused kernels skipped.
- The agention llama.cpp fork's trellis types are not present, so `native_hadamard_test`,
  `tq_cpu_parity` and `tq_cpu_parity_avx2` skipped. This build's ggml is the pinned upstream
  llama.cpp revision with the configure-time trellis MMQ overlay, not the fork tree.
- Multi-GPU was not tested (one card). Metal, SYCL and CUDA were not built (out of scope). No release
  packaging or app/Terminal end-to-end test.
- `isa_guard_check`'s two violations were confirmed by reading the sources and git history, not by
  running on a non-AVX-512 CPU (see next section).

## Findings for the merge and triage

1. Environment: on this PC the `docs/AMD_HIP.md` configure command produces an unknown HIP compiler id
   and the build then fails on the first `.cu` file. Adding `--rocm-path=/opt/rocm` (plus
   `--rocm-device-lib-path=/opt/rocm/amdgcn/bitcode` and `-L/opt/rocm/lib`) to `CMAKE_HIP_FLAGS`,
   or setting `ROCM_PATH=/opt/rocm` in the environment, fixes it. The `ROCM_PATH` variant was only
   verified through configure (`The HIP compiler identification is Clang 22.0.0`), not through a
   full build; the flags variant above was verified through configure, build, ctest and the smoke
   runs. This should be a line in `docs/AMD_HIP.md` for ROCm trees whose `llvm` directory is a
   symlink under `lib/`.

2. The merged branch compiles cleanly: 199 executable link targets, 0 compile errors, every fork and
   upstream file in the merged tree. Nothing in the build log names a missing or changed upstream API
   in `src/qwen35/`, `src/kernels/`, `src/prefill/`, `src/program/generate.cpp` or `frontend/`.

3. `isa_guard_check` fails with 2 violations on this build: `iq512_reference_gu_rows` and
   `iq512x_rows` in `src/kernels/cpu/iq_avx512.cpp` hold wide-ISA instructions but are not listed in
   `WIDE_PUBLIC` in `tools/check_isa_guard.py`. This is inherited from upstream 0.2.10, not from the
   merge resolution: both functions and the same allowlist are already present in the upstream merge
   parent `ffa80d73` (`git grep` and `git show ffa80d73:tools/check_isa_guard.py` checked). It is a
   guard/allowlist gap, not a runtime defect; the fix is to list the two functions there if they are
   meant to be wide (they are reached behind the CPU checks like their siblings, but that is static
   reading, unconfirmed by a run on a non-AVX-512 CPU).

4. `hip_prefill_mmq_parity` fails only when `STRATA_PREFILL_MMQ=ON` (the flag in the Lead's working
   command; the stock documented configure leaves it off and then does not register the test — the
   attempt-1 build's 89-test registration had no `hip_prefill_mmq_parity` entry). On HIP the K-quant
   MMQ instances are not compiled by this branch's HIP `strata_mmq` path: the test's synthetic Q2_0
   rows pass (relative L2 4.9e-4 to 1.3e-3), then Q4_K/Q5_K/Q5_1/Q6_K report "not built
   (STRATA_MMQ_KQUANTS off?)" and the screen ends with "host quantizer unavailable". Not a compile
   failure; the HIP path under that flag is incomplete for K-quants.

5. `conversation_copy_streams_test` fails on this box: the card's first copy stream plus event took
   151,552 KiB (148 MiB) of device memory against the test's "<16 MiB" assertion. That looks like a
   ROCm 7.2 runtime allocation behaviour, not a merge problem; the threshold may need a
   version/driver-specific allowance or the test should be a skip where the runtime allocates this
   much.

6. Three failures are missing model fixtures, not code: `ple_parity` (Q2_0 PLE GGUF),
   `expert_parity` and `pool_test` (34 GB `pack/full/experts.bin`). They are registered unconditionally
   and fail when the artifact is absent, where the other tests of that class skip.

7. The branch note `docs/work/RX9070XT-32GB-PERF-CONFIGS.md` gives 11 rows for the dense table and 69
   for the long-context table; the files in `tools/hip/` have 9 and 67 data rows. Low risk, but a
   profile or A/B statement based on the note should be re-checked against the files.

## Where the artifacts are, and what is still running

- Build: `/home/garyb/Mounts/NVME/.agent-work/build-hip-0210-fixed` (kept for follow-ups; 892 MB).
- Failing first attempt (evidence): `/home/garyb/Mounts/NVME/.agent-work/build-hip-0210`.
- Logs: `/home/garyb/Mounts/NVME/.agent-work/`: `configure.log`, `configure-fixed.log`,
  `build-j8.log`, `build-strata-j8.log`, `build-all-j8.log`, `ctest.log`, `hipblaslt-*.log`,
  `smoke-strata-help.txt`, `smoke-strata-device-selftest.txt`.
- Scripts: `/home/garyb/Mounts/NVME/.agent-work/scripts/configure-fixed.sh`, `build-target.sh`.
- llama.cpp checkout used: `/home/garyb/Mounts/NVME/.agent-work/build-hip-0210/_deps/strata_llamacpp-src`.
- Nothing is still running: no ninja, hipcc or clang process was left; every build and ctest job
  finished. The GPU shows no leftover engine process.

## Addendum (Lead, after this report's run): the isa_guard fix and the final ctest count

The `isa_guard_check` failure above was an inherited upstream allowlist gap, not merge damage. The Lead added the two
entry points (`iq512_reference_gu_rows`, `iq512x_rows`) to `WIDE_PUBLIC` in `tools/check_isa_guard.py` with the same
reasoning as their siblings (they run only behind `cpu_avx512_ok()` / the `x512_one_ok` probe; the functions and the
gap are identical in the upstream parent `ffa80d73`, added by #3416). Re-run on the same build library
(`build-hip-0210-fixed/libstrata_kernels_cpu.a`):

```text
check_isa_guard: libstrata_kernels_cpu.a: 777 functions, 135 hold wide-ISA instructions, 0 not allowed
```

exit 0, and `python3 tools/test_check_isa_guard.py` passes its 10 tests. A full re-run of ctest on the same build
after the fix: **197 tests, 178 passed, 14 skipped, 5 failed** (`ctest-after-isa-fix.log` in the work folder). The
five remaining failures are the four environment/fixture classes above (three missing model fixtures, the ROCm copy
stream allocation expectation, the HIP K-quant MMQ gap under `STRATA_PREFILL_MMQ=ON`); `isa_guard_check` is green.
