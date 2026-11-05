# Merge 0.2.10 audit: fork features vs upstream 0.2.6→0.2.10

Task: `task-2`. Branch `codex/rx9070xt-perf-0.2.10`, HEAD `5eab60ce` (merge of the fork
`023834f1` onto upstream `Niko1221/Strata` main `ffa80d73`; merge base `4b9e05a2`). **Static
analysis only**: this report comes from reading git objects and the working tree. No build, no
GPU run, no test was performed by this audit; the build/ctest evidence in §7 is the Lead's and the
build owner's, quoted only for corroboration.

## 1. Verdict

**No blocker and no silent loss of fork code was found in the merge.** The merged tree is
byte-identical to git's own three-way auto-merge for every file except `CMakeLists.txt` (the one
conflict, resolved by the Lead), all 60 fork-only files are byte-identical to the fork parent,
every substantive line the fork added to the 9 shared files is present in the merged blobs, and
every fork feature keeps its code path. The CMakeLists resolution is well formed. Version strings
are 0.2.10 / engine 0.1.56 in every authoritative place. The residual risks in §5 are inherited
upstream behaviour changes (intended, off/opt-in by default, or documented upstream fixes), not
merge damage.

Recommendation: **safe to build and test**. Before treating the dense/checkpoint paths as
qualified, run the pre-test gates in §6.

## 2. What the merge actually did (evidence)

| Check | Evidence |
| --- | --- |
| Merge parents | `git show -s --format=%P HEAD` → `023834f1` (fork) + `ffa80d73` (upstream); base `4b9e05a2`. 34 fork commits, 1441 upstream commits. |
| Only one conflict | `git merge-tree --write-tree 023834f1 ffa80d73` prints `CONFLICT (content): Merge conflict in CMakeLists.txt` and auto-merges exactly the 8 other shared files; no other conflict. |
| No other manual edits | Auto-merge tree `25d6a0b4…` vs HEAD tree `38dc2d6e…`: only `CMakeLists.txt` differs (3 insertions, 8 deletions = the resolution). |
| No lost fork lines | Scripted diff of merge-base→fork and merge-base→upstream: every added line from both sides is present in the merged file. Fork added 2,925 lines across the 9 files (2,658 of them `q36_engine.cu`), 0 substantive lines missing. |
| Fork files untouched | 69 files changed by the fork; 60 are **byte-identical blobs** to the fork parent. The 9 that differ are exactly the 9 shared files (CMakeLists + 8), where upstream changes were applied on top. |
| No merge markers | `git grep '^(<<<<<<<|>>>>>>>|=======$)'` (and `\|\|\|\|\|\|\|`): 0 hits anywhere. |
| No duplicated blocks | No suspicious repeated ≥12-line block in the 8 shared files; a duplicate-target/option scan of `CMakeLists.txt` finds no duplicate target or option (the 7 "duplicate" target names such as `strata_kernels_cpu`, `ds4_gg_bench` are in mutually exclusive backend branches and are the same set upstream `ffa80d73` has). |
| No duplicate case labels | `block_geometry` has one `case 10/19/147/148/149`; `dq_dispatch` one `case 10/19/147/148/149`; `Fmt<10>`, `Fmt<19>`, `Split<19>` once each. |

## 3. Findings

No **blocker** was found. The table lists what a static read did find, with severity
`risk` (inherited behaviour change to validate, not merge damage) and `note` (checked, no action).

| # | Location | Severity | Evidence | Suggested fix |
| --- | --- | --- | --- | --- |
| F1 | `src/qwen35/q36_engine.cu:5971-5972` (+ `include/strata/qwen35/sampling.hpp`) | risk | Upstream 0.2.7+ refactored the fork/shared sampler: `Sampling` moved to `strata/qwen35/sampling.hpp` and #4590 changed the penalty window — by default the penalties count the reply's tokens, not the prompt (`penalty_history_start(ids, pen_prompt)`; `STRATA_PENALTY_PROMPT=1` restores the old window). This applies to the fork's dense engine too. The fork's measured tables (`docs/MODEL_RESULTS.md`) were taken under the old window. | No code fix. For A/B against the old tables, set `STRATA_PENALTY_PROMPT=1`; otherwise expect sampled-run texts to differ from the pre-merge numbers. |
| F2 | `src/qwen35/q36_engine.cu:4858-4882` | risk | Upstream #4708 added `reset()` (line 4866) between the warm prompt and the small-prompt/decode/spec probes, and clamps the warm chunk to `ctx`. The fork's checkpoint preallocation runs before warmup (`prealloc_ckpts()` called in `run()` at :6088, warmup at :6347) and `drop_ckpts_after` returns snapshot buffers to `ckpt_spare` (:2701-2706), so the preallocated pinned buffers are **not** freed by the new reset. Logic is consistent; behaviour is upstream's intended fix. | No fix. Run `qwen35_checkpoints_test` and one `STRATA_Q35_CHECKPOINTS=N` serve session to confirm on the rebuilt binary. |
| F3 | `src/kernels/cuda/iq_kernels.cu` (`native_expert_grouped`), used by `src/qwen35/q36_engine.cu:4116,4300` | risk | Upstream 0.2.7-0.2.10 made EXPDEC default-on on gfx12 (gate/up, and Q2_0 down: `STRATA_EXPDEC`, `STRATA_EXPDEC_DOWN`) and added the nib / down-ws expert paths. Default mode/legacy arithmetic is preserved per upstream comments, but the qwen35moe (Tiel-Coder) expert path on this gfx12 card now uses a different default kernel set than at 0.2.6. | No fix. Re-measure the qwen35moe-on-Strata path; `STRATA_EXPDEC=0` gives the old kernel choice for an A/B. |
| F4 | `CMakeLists.txt:2115-2117` + `frontend/CMakeLists.txt:14` | note (pre-existing) | With `STRATA_NATIVE_SERVER=ON` and `STRATA_GGML_DIR` set, top level adds `${STRATA_GGML_DIR}/ggml` and `frontend/` adds the whole llama.cpp checkout (which adds ggml again). This pairing is **identical in the fork parent before the merge** (fork `CMakeLists.txt` line 1717 has the same call), so it is not merge-introduced; the native AppImage was built with it. | Build owner: confirm the `-DSTRATA_NATIVE_SERVER=ON -DSTRATA_GGML_DIR=…` configure of the merged tree still passes (the fork's llama.cpp checkout may carry a guard for this). |
| F5 | `README.md:103`, `AGENTS.md:18` | note | Installer file names still say `Strata-0.2.9-*` while the tree is 0.2.10/0.1.56. Both files are byte-identical to upstream `ffa80d73`, i.e. upstream's own pre-release state, not a merge artefact. There is also no `docs/release-notes-0.2.10.md` (newest is `0.2.7`). | Do not fix in the merge commit (upstream-owned text). Note for the release/version owner. |
| F6 | `CMakeLists.txt` `enable_testing()` ×8 | note | 7 are upstream's, each inside its own conditional backend/test block (same count as upstream parent); the 8th is the fork's, inside `if(STRATA_NATIVE_SERVER)` before `add_subdirectory(frontend)`, which is what makes `frontend/CMakeLists.txt`'s `add_test` calls work. No duplicate in any one configuration. | No change. |

## 4. The 9 shared files, file by file

| File | Fork change preserved | Upstream change applied | Merge verdict |
| --- | --- | --- | --- |
| `CMakeLists.txt` | `STRATA_NATIVE_SERVER` option + fatal without `STRATA_GGML_DIR` (:12-15), native-server block with `enable_testing()` + `add_subdirectory(frontend)` (:2964-2967), dense tests `qwen35_checkpoints_test` (:367-370), `physical_vram_test` (:1919-1921), `dense_repack_test` (:2188-2190), `dense_lowbit_test` (:2647-2650) | version 0.1.56 (:11), `STRATA_BUILD_SPLIT` option + `add_subdirectory(src/split)` (:2971-2976), all other 0.2.7-0.2.10 options/targets | **Correct** — see §7. |
| `include/strata/artifact/gguf_reader.hpp` | IQ1_S block geometry `case 19` (256/50) at :207; Qwen35Dense architecture guard at :858 (key list + optional `nextn_predict_layers`) | TQP5/6/7 type names, block geometries `case 147-149` | Correct; no duplicate case labels; `check_engine_architecture` (:880) still produces the `STRATA_Q35_DENSE=1` refusal message the fork test expects. |
| `include/strata/kernels/cpu/native_expert.hpp` | `dense_repack_q8` declaration at :55, `<vector>` include | `NativeArithmetic` enum + `native_gu_rows_invariant`/`native_down_rows_invariant` declarations | Correct (additive, disjoint). |
| `include/strata/kernels/iq_kernels.hpp` | `dense_lowbit_mmvq` declaration at :18 | `NativeExpertLayout::gu_nib`, `native_expert_layout_nib`, nib setters (`+19` lines) | Correct (additive). |
| `src/kernels/cpu/native_expert.cpp` | `dense_repack_q8` definition at :159 (after `native_experts_available`) | big dispatch refactor to `native_gu_rows_impl<NativeArithmetic>` + wrappers (`+138/-…`); legacy `native_gu_rows`/`native_down_rows` still exported | Correct: the fork function sits unchanged outside the refactored dispatch; both sides' lines present. |
| `src/kernels/cuda/dequant_bf16.cu` | `iq_only()` includes Q2_K/IQ1_S (:426), `iq_rows_bytes()` helper (:429) used by `dequant_f16` (:444) and `dequant_f32` (:461) | sm_80 `dequant_wide_on` default (#3779) | Correct; fork contract "Q2_K/IQ1_S keep `iq_row_bytes` at 0" holds — merged `iq_row_bytes` (`iq_kernels.cu:5496`) still has no case 10/19 and returns 0. |
| `src/kernels/cuda/iq_kernels.cu` | `Fmt<19>` (:859), `Fmt<10>` (:861), `Split<19>` (:1250), `dq_dispatch` cases 10/19 (:3430-3431), IQ1_S/Q2_K kernels and dequantizers, `dense_lowbit_mmvq` (:5474, calls `launch_mmvq<10/19>`; guard allows only n_in%256, ncols 1..8) | `+1012` lines of EXPDEC-down-ws / expert-nibble kernels and dispatch, TQP additions, `native_expert_layout_nib`; `launch_mmvq`, `Fmt`/`Split` primary templates and `iq_row_bytes` legacy cases unchanged | Correct by construction (additive both sides); the fork's specializations are defined before `launch_mmvq` (3665) and are not in upstream's `STRATA_*_FMTS` lists on purpose — the dense path calls `dense_lowbit_mmvq` explicitly from `q36_engine.cu:3364-3369`, so no dispatch-list entry is needed. |
| `src/program/generate.cpp` | `--families` prints `qwen35` only with `STRATA_Q35_DENSE=1` (:3213-3220); family switch sends `Qwen35Dense` to `strata::qwen35::run` (:3276-3284) | ~4,000 lines of upstream program/prefill/DFlash2/serve changes elsewhere in the file | Correct: fork lines are in the pre-engine area before any upstream state exists; the early `return strata::qwen35::run(...)` means upstream's later changes cannot affect the dense engine path. |
| `src/qwen35/q36_engine.cu` | 2,658 added lines: dense engine, checkpoints, gfx12 matrix-core attention, IQ1_S/Q2_K decode path, 1024-thread FFN norms, MTP sampled-draft fixes, Tiel-Coder opt-ins, `@FILE` ids | **Exactly** the upstream delta `git diff 023834f1 HEAD` shows: `#include strata/qwen35/sampling.hpp` (:33), warmup clamp+`reset()` (:4858-4882), `Sampling`/`apply_penalties` refactor (:5325,:5328), #4590 penalty history (:5971-5972). Nothing else. | **Correct merge.** The diff against the fork parent is only the upstream change; fork lines are untouched. |

## 5. Cross-file contracts checked for fork code (all hold)

- All direct includes of `src/qwen35/q36_engine.cu` exist and are **unchanged** upstream except
  additive ones: `fused_gdn.hpp`, `gfx_arch.hpp`, `native_moe.hpp`, `native_router.hpp`,
  `qwen35/engine.hpp`, `pin_budget.hpp`, `gfx12_platform_defaults.hpp` unchanged; `prefill/gemm.hpp`,
  `prefill/moe_mmq.hpp`, `iq_kernels.hpp`, `native_mmvq.hpp` additive only.
- `native_mmvq`, `native_q8_1_bytes`, `native_quantize_q8_1`, `native_expert_layout`,
  `native_expert_grouped` signatures are unchanged base→0.2.10 (checked declaration diffs); the q36
  engine's call sites still match.
- `include/strata/kernels/cpu/pool.hpp`: `run_split_multi_native` gained a **defaulted** parameter
  (`NativeArithmetic`, default `Default`) — old call sites compile and keep legacy arithmetic.
- The pinned ggml version is unchanged (`third_party/ggml/VERSION.txt` has no diff base→0.2.10), so
  the fork tests' ggml calls (`from_float_ref`, `to_float`, `ggml_row_size`) are still valid.
- `include/strata/qwen35/checkpoints.hpp` (fork, new) is standard-library only — no upstream contract.
- `include/strata/core/physical_vram.hpp` (fork, new) is the only definition of
  `physical_vram_free` in the tree.
- q36 kernels' `q8_1_finite.hpp` change (upstream #3948) defaults to the old division unless a TU
  defines `STRATA_Q81_FASTMATH_DIV` — the fork's q36 TUs do not, so their arithmetic is unchanged.

## 6. Must fix before tests

**None found.** The merge itself needs no repair. These are the gates the build/test owner should
run (they validate the inherited upstream changes, not a merge defect):

1. `STRATA_NATIVE_SERVER=ON -DSTRATA_GGML_DIR=…` configure of the merged tree (pre-existing ggml
   add pairing, F4).
2. `dense_lowbit_test`, `dense_repack_test`, `qwen35_checkpoints_test` (needs a GPU/ggml device; the
   ctest skip code is 77 for the first) on gfx1201.
3. One dense serve session with `STRATA_Q35_DENSE=1` and one with `STRATA_Q35_CHECKPOINTS=N`, and a
   sampled request to see #4590 (F1); set `STRATA_PENALTY_PROMPT=1` for the old window.
4. A qwen35moe (Tiel-Coder) start on gfx1201 under the new gfx12 EXPDEC default (F3).

## 7. CMakeLists resolution review (the Lead's edit)

- `project(strata VERSION 0.1.56 LANGUAGES CXX)` — engine version 0.1.56, matches `AGENTS.md`
  "engine 0.1.56" and `packaging/*.sh` `PKGVER=0.1.56`.
- `option(STRATA_NATIVE_SERVER …)` declared once (:12); its missing-`STRATA_GGML_DIR` fatal once
  (:13-15); its block appears once (:2964-2967) with **one** `enable_testing()` and **one**
  `add_subdirectory(frontend)`, correctly closed.
- `option(STRATA_BUILD_SPLIT …)` once (:2971) and its block once (:2971-2976) with
  `add_subdirectory(src/split)`, correctly closed; it comes after the native-server block (both are
  independent; `frontend/` needs only targets defined earlier, e.g. `strata_engine`,
  `strata_prefill`, `strata_spec`, `strata_qwen35` at :924, which exist).
- Block balance compared with both parents: HEAD `if`/`endif` 236/236 = upstream 234/234 + the
  fork's two blocks; `foreach`/`endforeach` 19/19; nothing dropped from either side (tails compared:
  fork's native-server block ends the fork file, upstream's split block ends upstream's file; HEAD
  ends with the split block).
- The only conflict hunks were the version line and the file tail; the resolution took upstream's
  version and kept both feature blocks. No duplicate `project()`, option, target, or
  `add_subdirectory` was introduced.

## 8. Version and doc consistency

| Place | Value | Verdict |
| --- | --- | --- |
| `CMakeLists.txt:11` | `VERSION 0.1.56` | correct (engine) |
| `AGENTS.md:3` | "Version 0.2.10, engine 0.1.56" | correct |
| `strata_cli/__init__.py:10` | `PRODUCT_VERSION = "0.2.10"` | correct |
| `apps/desktop/package.json`, `apps/host/package.json` | `"version": "0.2.10"` | correct |
| `packaging/build_hip_linux.sh:30`, `build_dsv41_hip_linux.sh:22`, `mac/build_dsv41_engine.sh:21` | `PKGVER`/`VERSION` = 0.1.56 | correct |
| `README.md:103`, `AGENTS.md:18` installer names | `Strata-0.2.9-*` | upstream state (identical to `ffa80d73`), not a merge mismatch — see F5 |
| `docs/release-notes-0.2.10.md` | absent (newest 0.2.7) | upstream state, not a merge mismatch |
| Older `0.1.55`/`0.1.54` mentions | comments, test fixtures, historical measurement notes | all in upstream-owned files byte-identical to `ffa80d73`; not stale merge output |

Fork docs are **not clobbered**: `docs/MODEL_RESULTS.md`, `docs/QWEN36.md`,
`docs/CONVERSATION_CHECKPOINTS.md` are byte-identical blobs to the fork parent; upstream changed
only upstream docs. They describe the fork's 0.2.6-era measured builds on purpose (measurement
provenance — AGENTS.md forbids rewriting numbers), so they should not be updated to 0.2.10 numbers
without new measurements. `docs/work/` other files untouched (only this audit is added).

## 9. Dense-engine argument path (`serve/server.py`, `strata_control`)

The fork never edited `serve/server.py`, `strata_control/` or `strata_cli/` (not in the fork's 69
changed files), so no fork line could be lost there. The dense-engine arguments are built by the
fork's native server, not by those files:

- `frontend/main.cpp:895-947` builds the engine argv: profile `args` are passed through verbatim;
  direct worker mode adds `--native`, `--max-context`, `--ple-gguf`, `--ple-io direct`, `--kv`,
  `--pool-workers`, `--spec`, `--mtp`, …; the profile's `env` (where `STRATA_Q35_DENSE=1` lives) is
  applied to the worker (`frontend/main.cpp:905-911`), and `frontend/tuning.hpp:7` keys the router's
  "dense" behaviour off that env var.
- Every one of those flags is still accepted by the merged engine:
  `--native … --max-context --pack --expert-cache --expert-profile --prefill --spec --spec-min-p
  --mtp --ple-gguf --ple-io --kv --pool-workers --vision --serve` all exist in the merged
  `src/program/generate.cpp`, and `src/qwen35/q36_engine.cu` still parses `--tokens/--force @FILE`
  (:6375-6388, help at :6221).
- For the record, upstream's rewrites of `serve/server.py` kept the generic paths the dense flags
  would use if a run goes through it: the config `args` pass through (`engine_args`, :3498) and the
  config `env` is applied to the engine (:4066).
- The `strata-native` link line (`frontend/CMakeLists.txt:18`) is unchanged and still resolves:
  `strata_engine` → `strata_core` (PUBLIC, carries `STRATA_HAVE_DFLASH2` and
  `src/platform/io_priority.cpp`), and `strata_qwen35` is conditionally linked when the target
  exists.

## 10. Fork features verified present in the merged tree (spot evidence)

| Feature | Evidence in merged tree |
| --- | --- |
| Dense Qwen35 / Saluki / Swift | `ModelFamily::Qwen35Dense` (`model_family.hpp:19,25`), layout/geometry (`family_layout.hpp:63,90,166,251`), architecture guard (`gguf_reader.hpp:858`), family switch (`generate.cpp:3276-3284`), dense code paths throughout `q36_engine.cu` (`:1567,2204,4040,6090`) |
| IQ1_S / Q2_K mmvq decode | `Fmt<19>/Fmt<10>` (`iq_kernels.cu:859,861`), `Split<19>` (:1250), `dq_dispatch:3430-3431`, `dense_lowbit_mmvq` (:5474), caller `q36_engine.cu:3364-3369`, dequant routing (`dequant_bf16.cu:426,429,444,461`), repack (`native_expert.cpp:159`) |
| gfx12 matrix-core prompt/decode attention defaults | `k_attn_prompt_wmma` (`q36_engine.cu:709`), opts `STRATA_Q35_ATTN_FUSED`/`STRATA_Q35_DECODE_WMMA` (:2251-2285), decode-wmma self-test (:6324) |
| FFN-branch 1024-thread norm blocks | `k_add_rms_q_wide` call `<<<T,1024>>>` (`q36_engine.cu:3181`, decl in `q36_kernels.cuh`) |
| MTP sampled-draft fixes | `spec_sampled` (:1985-1986), `STRATA_Q36_SPEC_SAMPLED` (:6318-6321), sampled `spec_step` path (:5977-5979) |
| Conversation checkpoints | `STRATA_Q35_CHECKPOINTS` (:1991,2022,2297,5909), prealloc (:2728-2748,6088), tests targets (:367-370), `docs/CONVERSATION_CHECKPOINTS.md` |
| Tiel-Coder opt-ins | `STRATA_Q36_SKIP_MTP` (:1716-1725), `STRATA_Q36_LEAN_ARENA` (:2054-2062), `STRATA_Q36_NO_MMQ` (:2361), `STRATA_Q36_EXPERT_RESERVE_MIB` (:4931-4950) |
| Native C++ server + router (`STRATA_NATIVE_SERVER`, `frontend/`) | all `frontend/*` byte-identical to fork parent; CMake block §7; dense arg path §9 |
| `--tokens` / `--force @FILE` | `q36_engine.cu:6375-6388`, help :6221 |
| Fork hipBLASLt tables | `tools/hip/gfx1201-qwen35-dense-hipblaslt-100202.txt`, `tools/hip/gfx1201-qwen35-long-context-hipblaslt-100202.txt` (byte-identical to fork parent) |

## 11. Corroborating build evidence (not produced by this audit)

The build owner's run reported by the Lead: the merged branch builds cleanly for gfx1201 (199 link
targets, 0 compile errors) and `ctest` 177/197 pass with the 6 failures classified as missing
fixtures / ROCm behaviour / inherited upstream gaps; the fork environment variables
`STRATA_Q35_CHECKPOINTS`, `STRATA_Q36_SKIP_MTP`, `STRATA_Q36_LEAN_ARENA`,
`STRATA_Q36_EXPERT_RESERVE_MIB`, `STRATA_Q35_DENSE` survive in the built binary. That is consistent
with every static finding here. The classification of the 6 ctest failures is outside this audit;
this report only confirms nothing in the merge removes those variables/paths.
