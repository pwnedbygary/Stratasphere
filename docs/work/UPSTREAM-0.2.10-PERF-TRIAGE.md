# Upstream 0.2.6 → 0.2.10 performance triage for the RX 9070 XT PC

What upstream (github.com/Niko1221/Strata, main `ffa80d73`, 0.2.10, engine 0.1.56) changed between
our fork's 0.2.6 base (`4b9e05a2`) and the merge, that could help or hurt this PC and these models.
Read-only git and docs analysis. No builds, no benchmarks. Written for the merge review.

**The PC:** AMD Radeon RX 9070 XT 16 GB (gfx1201, 16,304 MiB, desktop holds ~1.1-2.0 GiB), Ryzen 7 7800X3D,
30 GiB usable RAM, ROCm 7.2, Linux. Models: Qwen3.8-27B dense (Saluki 27B IQ2-mix + MTP, Swift 1.5 27B
IQ3_S/IQ3_XXS + MTP), Qwen3.8-Flash-Next (ISTA-DAS IQ3_S and Q2_0, Coder IQ1_M, Gyro-S), and
Qwen3.6-35B-A3B "Tiel-Coder"/"Cyber-Tiel" UD-Q4_K_XL (21.7 GB, run today on llama.cpp Vulkan with CPU expert
layers).

**Range:** `git log 4b9e05a2..ffa80d73`. That is 1,441 commits and the tags v0.2.7, v0.2.7.1, v0.2.8,
v0.2.9, v0.2.10. Upstream keeps release-note files only up to `docs/release-notes-0.2.7.md`; the 0.2.8-0.2.10
notes are not in the repository, so their evidence here is commit messages, docs and source.

**Status labels used below**

- **default after merge** - on with no flag or env var (the merge kept upstream's default decision).
- **opt-in** - off unless a flag/env var says otherwise.
- **not for us** - the change only touches hardware, cards or paths this PC does not use.
- **static analysis only** - I read the code and docs and found no measured number for this exact change; the
  direction is argued from the code, not measured.
- Every speed number is quoted from a commit message or a repo doc, with the hardware that doc names. Where the
  doc does not name hardware, the row says so. Nothing here is my own measurement.

**How to read "transfers to 16 GB gfx1201":** RDNA4 ISA and gfx12 kernel defaults are the same on a 16 GB
RX 9070 XT as on a 32 GB R9700, which is the card most gfx12 numbers were measured on. The 16 GB card differs
in one way that matters: it must stream many experts from RAM/SSD, so changes that use idle CPU cores or cut
host traffic pay more here, changes that assume "every expert fits" pay less, and the 26.8 GiB PLE table cannot
live in RAM at all (section D).

---

## The ranked list

Rank is expected value for this PC, for the models listed above. "Applies" is my judgement from the change's
own path and from the docs; "Measured on" quotes the source.

| # | Commit | Area | One-line change | Measured numbers and hardware | Applies to 9070 XT? | Status after merge | Source |
|---|---|---|---|---|---|---|---|
| 1 | `02c367d5`, `b9e3e2b8` | gfx12 prompt experts | Fused IQ2_S / IQ4_XS gate/up prompt kernels are now ON by default on Linux gfx12 (they were opt-in), so the IQ3_S pack's 21 layers that keep MMQ now fuse too; gfx12 CPU-share limit drops to 1,536 tokens | R9700 (p620, IQ3_S, 54% resident, 5 interleaved pairs): prompt 2K +10.8%, 4K +12.2%, 8K +13.2%, 32K +10.0%; 1K and 1.5K unchanged. First-token KL vs the FP16 prompt path 0.0150 vs MMQ's 0.0263. Quick quality suite lost 2 of 19 tasks when measured opt-in | Yes. Flash-Next IQ3_S/Q2_0 packs contain those layers. Same kernel path on gfx1201 | **default** (Linux; `STRATA_PF_FUSED_IQ2S=0` or `STRATA_PF_FUSED=0` off). Windows still opt-in | `src/prefill/moe_fused_iq.cu` `gfx12_iq2s_default()`, `include/strata/core/gfx12_platform_defaults.hpp`, commit `b9e3e2b8` |
| 2 | `d2068bde` | gfx12 prefill | CPU prefill share armed by default for a single gfx12 card (serve, one card, no batch slots, experts missing) | R9700 (p620, IQ3_S, 54% resident): prompt 500 +18.3%, 1K +30.6%, 2K -0.19%, 8K -0.18%; first-token KL 0.022 at 1K, top-1 29/29 | Yes if launched through the server (`strata launch`/`serve/server.py`) and the chunk is under the share limit. A 16 GB card misses experts, so the condition holds | **default** (Linux gfx12). `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026`, `STRATA_PREFILL_CPU_SHARE=0` off | `include/strata/core/arch_defaults.hpp` (`gfx12_prefill_share_default`), `include/strata/prefill/share_rules.hpp` |
| 3 | `8d56d417` (also `d7ad4a27`, `7901bc51`) | gfx12 decode | Native packs start the decode miss split at `pcie_frac` 0.15 (was 0.55). More missing experts go to the idle CPU pool | R9700: 8K decode 86.1→94.6 tok/s (+9.9%), 32K 67.2→84.4 (+25.6%), 64K +4.7%; under serve a short chat 84.4→93.2 (+10.4%). First-token KL 0.0013 vs 0.55, same top-1 12/12 | Yes. Changes bits slightly; this is the default trade upstream took | **default** on gfx12. Explicit `--pcie-frac N` wins; `STRATA_PCIE_FRAC_DEFAULT=old` restores 0.55 | `include/strata/core/defaults_026.hpp`, `docs/AMD_HIP.md` RDNA4 section |
| 4 | `1f8ca8de` | RDNA4 kernels | Hand-ported the RDNA4 routed-expert, draft-layer, shared-expert and head kernels (PR 3259/3264, author bsorensen110). Some LDS-only barriers, S2 and head changes were run on gfx1201 only | No user-facing number in the commit; it says the S2/head changes "were only run on gfx1201" and lists parity tests. **Static only here** | Yes: these are the shared verify-window, draft-layer, shared-expert and head kernels the Qwen3.8 engine uses, compiled for gfx12 | **default** (gfx12-only code; `STRATA_GFX12_DEFAULTS=0` gives the old behaviour) | commit `1f8ca8de`; `src/kernels/cuda/s2_expert_grouped.cu`, `fused_gr.cu`, `shared_expert.cu`, `mtp.cpp` |
| 5 | `29e8a47a`, `b353ce7d`, `13edeb7f`, `d55138e9` | DFlash2 drafter on HIP | DFlash2 drafter now runs on the HIP engine, one drafter per run, auto-picked for Qwen3.8-Flash-Next/Gyro/Unsloth; gfx12-specific Q4_0 GEMM variants; confidence steps by expert slots | gfx1201 is a tested card: +7.2% short, +1.35% long (commit `29e8a47a`). On R9700 (gfx1201, p620) the gfx12 GEMM variants cut drafter kernels (gate+up 56→44.5 us, down 39→23.4 us) and gave +1.0% tok/s in a serve A/B. The headline comparison was on RTX PRO 5000: +7.8% short / +1.1% long vs MTP, faster on 21 of 23 prompts | Yes for Flash-Next (incl. ISTA-DAS IQ3_S/Q2_0) on one card, one request. Not for Swift/Saluki dense or Coder | **default choice** where it applies (drafting `auto`), only on engines with DFlash2 code (HIP has it); falls back to the MTP when not usable | `strata_control/drafting.py` (`DFLASH2_BACKENDS`, `_TESTED` has `("hip","gfx1201")`, `SUPPORTED_FAMILIES`), commit `d55138e9`, `29e8a47a` |
| 6 | `3b34b6c7` | PLE table | Keep the n-gram (PLE) table in RAM when the PC can afford it; opt-out `STRATA_PLE_AUTO_RAM=0` | R9700 (p620, IQ3_S): 8K prompt PLE stage 845→132 ms, prompt 2125→2611 tok/s, answer md5 identical. Release notes 0.2.7 quote one R9700 32 GB / 125 GB RAM: 8K prompt 2,047→2,505 tok/s (+22%), 32K 2,491→2,802 (+12%). A 32K table 26.8 GiB | **No on this PC as a default** (table + expert arena + 8 GiB does not fit 30 GiB; see section D). Only if `--ple-io ram` were forced, which cannot work here | **default where RAM fits**; the policy silently declines here, so direct reads stay | `include/strata/core/ple_residency.hpp` (`ple_resident_admit`), `docs/AMD_HIP.md` RDNA4 section, `docs/release-notes-0.2.7.md` |
| 7 | `3fd5a748`, `82eaeb42`, `09982fff`, `db1bfaaa` | gfx12 decode defaults | Four exact decode switches on gfx12; `STRATA_HEAD_MIX_MULTI=1`; three bitwise load-ordering variants; MTP draft layer's q8_1 images on gfx12 | R9700 (p620, IQ3_S, engine 0.1.51, 8K prompt, rotated pairs): the four together +3.5% decode, 3/3 pairs, answer md5 identical (one at a time: ATTN_LANECELL+ATTN_MERGE_V2 +1.9%, TSUM +1.9%, EXPDEC +0.6%, GDN_SPLIT 0.0%). HEAD_MIX_MULTI: R9700 serve, json_extract +0.82% (6/8), agent +0.63% (6/8), code -0.96% (A/A noise ±3.5%), greedy ids identical 16/16 and 15/16 | Yes, dense and MoE decode both read these kernels | **default** on gfx12 Linux; `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` or the single variable turn each off | `src/core/arch_defaults.cpp` gfx12 table, `include/strata/core/gfx12_extra.hpp`, commits `3fd5a748`, `82eaeb42`, `09982fff` |
| 8 | `4d9d9cef` … `ac3e0e17`, `bee469c4` | gfx12 GDN prompt | Chunked WMMA GDN recurrence on gfx12; the server turns it on for Qwen3.8 on Linux HIP gfx12 only | Final gate at 64 tokens: +1.7% to +6.1%, KL 0.0025/0.038, 55/61 tasks, soak clean (R9700; commit `ac3e0e17`). Server comment: +3.5% at 4K, +5.5% at 16K prompts, suite 55 vs 56, KL mean 0.0025 (R9700) | Yes. Flash-Next only (the server sets it for the `qwen4exp` engine), one gfx12 card | **default via the server** for those starts. Engine default stays off; `STRATA_GDN_CHUNKED=1/0` wins | `serve/server.py` `GDN_STACK_GFX12`, commit `bee469c4`, `ac3e0e17` |
| 9 | `c31f1fa3` | gfx12 long-prompt read | Matrix-core QSA block scorer (`STRATA_SELECT_WMMA`) past 28,672 tokens is now the gfx12 default; below the floor the read stays bit-identical | R9700 (p620, IQ3_S, serve, interleaved pairs, 3 pairs): 32K prompt +1.83% (3/3), 98K +5.74% (3/3); first-token KL on seven 30K-96K prompts mean 0.0018, max 0.0055, top-1 7/7; needle 25/25 | Yes for prompts above ~28K. Its answers change there; ordinary prompts keep exact answers | **default** on gfx12. `STRATA_SELECT_WMMA=0`, `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` off | `include/strata/core/gfx12_extra.hpp`, commit `c31f1fa3` |
| 10 | `2970fe74`, `14a85eca`, `3410a858` | QSA prompt attention | Matrix-core prompt attention: 122 fewer LDS bytes a block (12 real query rows, 72-byte V rows, chunk id prefetch); norm+rope fused; attention gate in the epilogue | Kernel time on gfx1151, int8 KV, `STRATA_PA_FAST`: 32K context / 2,048 queries 11.05→9.95 ms; 8K / 4,096 queries 21.76→18.90 ms. Fusions are bit-exact; NR drops traffic ~1.1 GB→0.4 GB per QSA layer at 8K. No whole-model number measured on RDNA4 | Yes for Flash-Next prefill (HIP_WMMA path is on by default on gfx12 Linux with int8 KV). gfx1201 itself not measured for these two levers | **default** where the matrix-core prompt attention runs; `STRATA_PA_TRIM=0`, `STRATA_PA_RPF=0`, `STRATA_QSA_FUSE_NR=0`, `STRATA_QSA_FUSE_GATE=0` off | commits `2970fe74`, `14a85eca`, `3410a858`, `src/prefill/prefill.cpp` |
| 11 | `ee455af7` | setup / KV streaming | Setup streams the KV cache from 64K up when the PC has room for the experts + 10 GB, not only for an all-in-RAM model | 12 GB mimic (l620, IQ3_S, 45 GiB cap): 756→1,267 expert slots, 32K prompt 21-34 s→12-17 s, 118K prompt 76-122 s→45-63 s, decode flat to +10%; owner's RTX 5070: 924→1,585 slots, 32K prompt 130→79 s | Yes in principle for a 16 GB card with 128K context: more expert slots, faster hybrid prompts. Only applies when setup writes the config; a hand config keeps whatever it has | **new setup default** (0.2.10). `--kv-streaming on|off` unchanged | commit `ee455af7` |
| 12 | `9d3c8c36` | HIP verify windows | HIP builds take verify/batch windows of up to 16 rows (`STRATA_WINDOW_MAX_T=auto`); windows of 8 run the old kernels bit-for-bit | No AMD decode number measured. It is a capacity change for layer splits and batch groups: 4×R9700 64 slots in 4 groups of 16; `--batch 16 --batch-groups 1` one 16-row window; exactness tests 64/64 and 16/16 tokens. Cost: bigger binary 44.9→53.7 MB, +7 MiB VRAM a card, 30-45 ms a server start | Only for slot-heavy or multi-card use. One card, one request gains nothing measured | **default** in a HIP build (`auto`→16). `-DSTRATA_WINDOW_MAX_T=8` restores 0.2.7 | commit `9d3c8c36`, `docs/BATCHING.md`, `include/strata/kernels/window_limits.hpp` |
| 13 | `076a195b`, `c3c76f0a`, `2e7db84b` (with `2a104287`, `8cb6e82d`, `e4be9e50`) | fit / VRAM plan | `fit_start` now prices a discrete HIP card like CUDA: HIP base 3,900 MiB measured on R9700, walks `--kv-resident` 20480/8192/4096 only; never changes context or request count | R9700 (p620) squeezed by a memory hog, DFlash2 alone, 128K, `--kv-resident 32768`: 7942 MiB→787 slots, 48-52 tok/s; 7000→439 slots; 6400→273; 5800 head to host (31 tok/s); 5200/4600 refused with a plain line. An 8 GB AMD card starts as set up; a 6 GB one walks the window | Yes as a safety net. On a 16 GB card with 128K int8 KV the estimate leaves ~9 GiB for the expert cache, so nothing changes for a normal start. For Qwen3.6 the family is unmeasured, so `fit_start` leaves it alone | **new default** in `serve/server.py` for HIP starts (0.2.10). User args are kept | `strata_control/vram_fit.py` (`fit_start`, `KNOBS`, `BASE_SLOTTED_MIB`), commit `076a195b` |
| 14 | 0.2.7 release notes | KVA long prompts | Approximate the last 12 layers of a very long cold prompt (`STRATA_KVA=1`); off by default | Radeon R9700 (gfx12) IQ3_S, 27K needle prompt: prompt 2,411→3,086 tok/s (+28%), decode 94.7→84.3 tok/s (-11%); needle found with and without. RTX 5070: +11% at 14K, +18% at 30K, but answers can lose details | Yes, Flash-Next only, one card, no batch/DFlash2, prompt >= 4,096 tokens. It is a quality trade, not a free win | **opt-in** (`STRATA_KVA=1`; the app's "Faster long prompts") | `docs/KVA.md`, `docs/release-notes-0.2.7.md` |
| 15 | `a3b7d9de` + `tools/hip/gfx1201-*.txt` | hipBLASLt table | A hand-built HIP engine uses plain hipBLAS unless pointed at a tuning table (`STRATA_HIPBLASLT_TUNING` or `<engine>/tuning/<arch>-hipblaslt-<version>.txt`) | R9700, `gfx1201-hipblaslt-100202.txt` (hipBLASLt 1.2.2), full IQ3_XXS, `--kv int8 --kv-resident 32768`: 32K prompt 638→1,177 tok/s, 7K 656→1,164 tok/s. The 1.5.0 table: 4,210-token prompts 1,531→1,590 (+3.9%). 1.2.0/1.4.1 tables gave 0-3% end to end on R9700 | **Only if our build links the system hipBLASLt and the version matches a shipped table.** ROCm 7.2 with the system hipBLASLt 1.2.2 (the docs' ROCm 7.2/7.2.4 rows report 1.2.2; verify the loaded version) matches `gfx1201-hipblaslt-100202.txt`. A release-style build that bundles its own hipBLASLt may not | **opt-in by build/env**; setup copies the matching table into release packages | `docs/AMD_HIP.md` "Tuning table", commit `a3b7d9de` |
| 16 | `472de9cd`, `587a4744`, `5a13c34f`, `eb891724`, `07d5ecfa`, `067b7433`, `43543e9b`, `07fb0e71`, `b537586e` | PLE / RAM shortfalls | PLE residency only admitted with table+arena+8 GiB; lock given back when it starves the conversation cache; short RAM falls back to file reads or a RAM copy of the hottest experts instead of stalling; one `--ram-keep-gib` for both RAM decisions | PLE: no speed number, these are safety fixes (the 0.2.7 rule had admitted the 26.8 GiB table on a 64 GB Windows PC and the expert load died with error 1453). Arena fallback RAM copy: RTX 5070 PC, 128K, 44.6 GiB available: 2,000-token answer 25.2 tok/s vs 9.4-10.1 without; lowest available RAM 4.05 GiB. The first fallback commit says "not built, not run: the patch is untested" | Only if the expert arena does not fit RAM. This PC has 30 GiB RAM and Flash-Next's full arena is 46.84 GiB, so the fallback path is plausible; the speed number is from an RTX 5070 (NVIDIA) and the patch itself was untested at merge | **default** fallbacks (`STRATA_ARENA_RAM_FALLBACK=0` off, `STRATA_GFX12_DEFAULTS` unrelated); `--ram-keep-gib` new | `include/strata/core/ple_residency.hpp`, commits `472de9cd`, `067b7433`, `07fb0e71`, `43543e9b`, `b537586e` |
| 17 | `cfa197e2` | Qwen expert cache | The automatic expert cache now prices the ~245-256 MiB the load allocates after it, so the card keeps the reserve the launch asked for | RTX A4000 16 GB / A6000 48 GB: the card ended ~240 MiB under `--vram-reserve-mib 700` before; the cache is ~100 slots smaller (~1.8% on a 16 GB card). Not new in 0.2.7, present in 0.2.6 too | Yes, all Qwen starts. Small capacity cost, keeps the requested reserve | **default** (0.2.8) | commit `cfa197e2`, `include/strata/core/auto_cache.hpp` |
| 18 | `4c5a5549` | IQ4_XS decode | IQ4_XS 5-column interleaved MMVQ runs as 6 columns with 5 stored (`STRATA_IL_PAD`), bitwise | No speed number in the repo. Bitwise by construction (`native_mmvq.cu`: "bitwise") | Yes for IQ4_XS down layers | **default** (`STRATA_IL_PAD` default 1) | `src/kernels/cuda/native_mmvq.cu`, commit `4c5a5549` |
| 19 | `7d4feb2e` | Decode T=5 | Hyper-connection down read takes two rows a warp at T=5 instead of the batched-staging variant | Screen: 15.5 against 16.2 us; bitwise the same dots (hardware not named; likely RDNA4 from the branch) | Yes for MTP verify windows of 5 rows (spec 4) | **default** (where the screen said so) | commit `7d4feb2e` |
| 20 | 0.2.7 release notes | MTP text | `--mtp-draft-vocab draft_vocab_en.bin` | +2.7 to +3.1% decode for English text; the release notes do not name the card for this bullet | Yes (MTP path, all backends) | **opt-in** | `docs/release-notes-0.2.7.md` |
| 21 | `ac3e0e17`-related, `84aa2d7d` | GDN crossover | Chunked GDN runs from 64 tokens (gfx12) | The +1.7% to +6.1% gate above | Yes, same as row 8 | **default via server** | commits `ac3e0e17`, `84aa2d7d` |

Rows deliberately left out of the table because they do not help this PC are in "Not for us" below.

---

## A. What is ON by default on this gfx1201 card in 0.2.10 after the merge

Checked in the merged tree (HEAD `5eab60ce`; the fork side does not modify these files, and `git diff ffa80d73 HEAD`
shows no change to `src/core/arch_defaults.cpp`, `include/strata/core/*` or the per-architecture default tables). The Lead asked
for this exact list.

| Setting | State on RX 9070 XT, Linux, int8 KV, 16 GB | Since | Gate / off switch | Where |
|---|---|---|---|---|
| `STRATA_PF_FUSED` (fused int8 prompt experts for IQ3_S / IQ3_XXS / Q2_0 gate/up) | **ON** | 0.2.6 (inherited, not new in the merge) | Linux only (`kGfx12PromptDefaultsOn = !windows`); `STRATA_PF_FUSED=0` off; needs a HIP build with `-DSTRATA_PREFILL_MMQ=ON` + native experts | `include/strata/core/gfx12_platform_defaults.hpp`, `src/prefill/moe_fused_iq.cu` |
| Fused IQ2_S / IQ4_XS gate/up (the new part) | **ON** | 0.2.10 (`02c367d5`) | Linux gfx12 only; `STRATA_PF_FUSED_IQ2S=0` or `STRATA_PF_FUSED=0` off | `gfx12_iq2s_default()` |
| `STRATA_HIP_WMMA` (matrix-core prompt attention) | **ON** where the KV is int8 | 0.2.6 (inherited) | gfx12 Linux default; Windows opt-in `=1`; `STRATA_HIP_WMMA=0`/`auto` | `docs/AMD_HIP.md` "Speed switches" |
| `STRATA_ROUTE_TAIL_SKIP` (rank 7) | **ON in server starts** on gfx1200/gfx1201 when experts are missing and not nearly all cached; off with `--batch` slots, a peer card, NVFP4 experts | 0.2.6 (inherited) | Serve start-up sets 7; `STRATA_ROUTE_TAIL_SKIP=0`/`=7` wins; `STRATA_TAIL_SKIP_MAX_CACHED` moves the "nearly all cached" line (default here 90%, 60% on an AMD layer split) | `src/program/generate.cpp` serve block, `src/core/expert_source.cpp`, `docs/DETAILS.md` "On by default" |
| PCIe miss split 0.15 (`--pcie-frac`) | **ON** (0.15) for native packs | 0.2.7 (`8d56d417`) | `--pcie-frac N` wins; `STRATA_PCIE_FRAC_DEFAULT=old` restores 0.55; `STRATA_DEFAULTS=026` | `include/strata/core/defaults_026.hpp` |
| `STRATA_SCORES_THREAD` (exact long-prompt QSA scorer) | **ON** for batches >= 64 queries with >= 16,385 live index blocks | 0.2.6 (inherited) | gfx12 default; `STRATA_SCORES_THREAD=0` off | `src/kernels/cuda/qsa_select.cu`, `docs/AMD_HIP.md` |
| `STRATA_HIP_QUEUE_PRIME` (ordered queue opening at start) | **ON** | 0.2.1 (inherited, not in this merge range) | gfx1200/1201 Linux; `STRATA_HIP_QUEUE_PRIME=0` off | `src/program/generate.cpp`, `docs/AMD_HIP.md` |
| `STRATA_HC_Q8` (8-bit hyper-connection weights) | **ON** | inherited (0.2.6 base already has the gfx12 rule) | on for gfx12 and gfx1151; `STRATA_HC_Q8=0` or `STRATA_GFX12_DEFAULTS=0` off | `src/core/native_dense.cpp` `hc_q8_env()` |
| `STRATA_SELECT_WMMA` + floor 28,672 | **ON** (0.2.10) | 0.2.10 (`c31f1fa3`) | answers change above ~28K; `STRATA_SELECT_WMMA=0`, `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` | `include/strata/core/gfx12_extra.hpp` |
| `STRATA_HEAD_MIX_MULTI` | **ON** (0.2.10, exact) | 0.2.10 (`82eaeb42`) | `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` | `include/strata/core/gfx12_extra.hpp` |
| gfx12 extra decode table (`STRATA_SMALL_UNR=3`, `GR_Q32_G=2`, `GR_Q8_PFS=1`) | **ON** (0.2.10, bitwise) | 0.2.10 (`09982fff`) | `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` | `src/core/arch_defaults.cpp` |
| Four decode switches (`ATTN_LANECELL`, `ATTN_MERGE_V2`, `TSUM`, `EXPDEC`) | **ON** | 0.2.7 (`3fd5a748`) | `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` | `src/core/arch_defaults.cpp` |
| CPU prefill share on one gfx12 card | **ON** (server starts) | 0.2.10 (`d2068bde`) | one card, no batch slots, experts missing, chunk under the share limit (1,536 tokens when the IQ2_S fused default is on); `STRATA_PREFILL_CPU_SHARE=0`, `STRATA_GFX12_DEFAULTS=0`, `STRATA_DEFAULTS=026` | `include/strata/core/arch_defaults.hpp`, `src/prefill/prefill.cpp` |
| GDN chunked on gfx12 | **ON via the server** for Qwen3.8 Flash-Next (not in the engine by itself) | 0.2.9 (`bee469c4`) | Linux, HIP, every visible card gfx12, `arch == qwen4exp`; explicit env wins; `STRATA_GDN_CHUNKED=0` off | `serve/server.py` `GDN_STACK_GFX12` |
| `STRATA_IL_PAD` (IQ4_XS 6-column MMVQ) | **ON** (bitwise) | 0.2.10 (`4c5a5549`) | `STRATA_IL_PAD=0` off | `src/kernels/cuda/native_mmvq.cu` |
| DFlash2 as `auto` drafter | **ON where it applies** (Flash-Next family on HIP, one card, one request) | 0.2.10 | `--drafting mtp/off`, `--draft mtp`, or a model outside the supported families | `strata_control/drafting.py` |
| fit_start on discrete HIP | **ON** (server starts) | 0.2.10 (`c3c76f0a`/`076a195b`) | skips layer split, DeepSeek, Metal, APU, and any model family it has no measured base for | `strata_control/vram_fit.py` |
| `STRATA_PLE_AUTO_RAM` | **decides "no" on this PC** (see D) | 0.2.7 | on where table + arena + 8 GiB fit; `STRATA_PLE_AUTO_RAM=0` off | `include/strata/core/ple_residency.hpp` |
| `STRATA_PREFILL_ROUTED_STREAM` | off | 0.2.10 opt-in | single GPU, streamed chunk >= 1,024 tokens; `=1` on, `_DEPTH` 1..256 | `include/strata/prefill/routed_stream.hpp` |
| `STRATA_LM_HIP` (layer-major read on HIP) | off | 0.2.9 opt-in | `=1` on; not qualified on RDNA4 | commit `1d9b4c48`, `docs/DETAILS.md` |
| `--lookup-max-t` (12 by default) | **N/A on HIP** | 0.2.10 | CUDA, one conversation, one GPU, 8-row CUDA build only | `docs/DETAILS.md`, commit `cc703b5e` |
| `STRATA_MTP_ROOM_MIN` | **N/A**: CUDA only | 0.2.10 opt-in | CUDA `--serve` no `--batch` | `docs/DETAILS.md` |
| `STRATA_WINDOW_MAX_T` | **16** (HIP auto) | 0.2.8 | `-DSTRATA_WINDOW_MAX_T=8` builds 0.2.7 windows | `include/strata/kernels/window_limits.hpp` |

**Card-size gating:** none of the settings in this table is turned off on a 16 GB card. Card size only
decides whether a change can engage: the miss split and tail skip need experts that are not resident (a 16 GB card
qualifies), PLE auto-RAM needs RAM (30 GiB here does not qualify), and the fused prompt kernels need a native pack
of a covered format. Linux vs Windows is the real gate: the gfx12 prompt defaults (HIP_WMMA, PF_FUSED, fused
IQ2_S/IQ4_XS) are Linux defaults and opt-in on Windows.

**Build flags that gate rows 1-2 and 4:** the fused prompt kernels and the native expert paths are compiled only
when the HIP build is configured with `-DSTRATA_PREFILL_MMQ=ON` (and `STRATA_NATIVE_EXPERTS=ON`); the CMake option
defaults to OFF and upstream's own note says release builds pass `MMQ=ON`. If our merge build left it off, the
fused-expert rows do nothing (the engine logs and uses MMQ/portable paths). **The Lead should verify the local
CMake cache** (`STRATA_PREFILL_MMQ`, `STRATA_NATIVE_EXPERTS`). This is an environment fact, not a git fact.

---

## B. Wider verify windows (commit `9d3c8c36`) - what they change, and what they do not

- **What it changes.** A verify or batch "window" is one launch that checks N draft tokens/rows. The maximum was
  8 rows. HIP builds now take up to 16 (`STRATA_VERIFY_MAX_T=16`; the Lead's cache has it). The per-row arrays,
  expert tables, resident plan and grouped-expert kernels are sized from one number. A window of up to 8 rows
  runs the same kernel instantiations as before, bit for bit (HIP gfx1201: 3,096 of 3,097 kernels identical, the
  other a debug variant).
- **Why it exists:** on a layer split each pipeline group is its own window, so 32 slots on four cards were four
  groups of 8 and nothing larger could run. It also enables `--batch 16 --batch-groups 1` on several cards.
- **Decode gain on AMD: none was measured.** The commit and `docs/BATCHING.md` give capacity and exactness
  (64/64 tokens on 4×R9700, 16/16 on one R9700), not a one-card decode gain. The cost is real: the HIP binary
  grows 44.9→53.7 MB, +7 MiB VRAM a card, 30-45 ms once per server start.
- **What max useful `--spec` is now:** on one card the server still caps requests at once at 8 (`serve/server.py`:
  `PARALLEL_MAX = 8`, "without a layer split the engine's batch window holds at most 8 rows"). The
  MTP verify window is `--spec + 1` rows, so on the old 8-row path the useful maximum was `--spec 7`; the 16-row
  HIP build would in principle allow `--spec` up to 15, but **upstream measured no decode gain for wider windows
  on one card**, and the shipped/measured settings are `--spec 3`/`4` (setup's MTP) and `--spec 6` (DFlash2).
  Recommendation: keep the current `--spec`; do not raise it for the 16-row build. This conclusion is static
  (no repo measurement of one-card 16-row decode).
  `--mtp-max-t M` (cap the MTP's own windows, 0 = `--spec`) is unchanged and still useful when a long
  suffix match should outrank the MTP's window.
- **New opt-ins that could apply to HIP/one GPU:** `--lookup-max-t` does **not** (CUDA only, per `docs/DETAILS.md`);
  `--suffix-draft` is the older suffix drafter and is not changed here; `--mtp-draft-vocab` does apply and is
  quoted above (+2.7-3.1% English decode); `--batch-mtp` is the existing opt-in for drafts in batch slots
  (`docs/QWEN36.md`); `STRATA_MTP_ROOM_MIN` and `STRATA_PIPED_MTP` are CUDA/layer-split only.
- **Can be dropped:** if VRAM is tight, `-DSTRATA_WINDOW_MAX_T=8` restores the 0.2.7 binary and gives the +7 MiB
  back. Not worth it unless something else needs the MiB.

---

## C. DFlash2 on gfx1201

**What it is.** A block drafter trained for Qwen3.8-Flash-Next. The engine loads either DFlash2 or the MTP layer,
never both (`strata_control/drafting.py`: "One drafter per run", owner decision 2026-11-02). DFlash2 for one card
and one request at a time; a layer split or several requests at once keep the MTP.

**What it needs.** The pinned `incoai/Qwen3.8-Flash-Next-DFlash2` repository (1.2 GB safetensors download), packed
on this PC into a 522,458,112-byte Q4_0 file (`tools/dflash2_pack.py --q4_0`). `setup`/`Load` can also convert the
selector codebooks to Q4_0, which cuts the drafter from 890 to 715 MiB VRAM (174 MiB less; the same drafts and
text). The engine flags are `--draft dflash2 --draft-model <pack> --draft-fallback none --mtp-draft-vocab <file>
--spec 6 --draft-conf 0.35`, with no `--mtp`. The MTP layer is 1,082 MiB, DFlash2 890 MiB (715 converted), so
DFlash2 frees 192-367 MiB of VRAM for the expert cache.

The DFlash2 code is compiled into every CUDA and HIP engine build (`CMakeLists.txt` sets
`STRATA_HAVE_DFLASH2=1` for them); the merged HIP build has it. Our build needs no extra flag for it.

**Which models get it from `auto`.** `SUPPORTED_FAMILIES = ("qwen", "gyro", "unsloth")` and the run-config name
pattern `qwen3.8-flash-next[...]`. That includes ISTA-DAS's IQ3_S and Q2_0 packs (family `qwen`, names like
`qwen3.8-flash-next-iq3_s`). It explicitly does **not** include Swift (UkisAI), Coder (ISTA-DASLab coding version)
or NVFP4: those start the MTP. So the Coder IQ1_M stays on MTP; the IQ3_S/Q2_0 Flash-Next sets can use DFlash2.

**Measured numbers.**
- On gfx1201: "gfx1201 is a tested card (+7.2% short, +1.35% long)" (commit `29e8a47a`, PR-AMD pass; the commit's
  surrounding work was measured on the R9700 p620). `strata_control/drafting.py` lists `("hip","gfx1201")` in
  `_TESTED`.
- The headline comparison upstream publishes: RTX PRO 5000 (Blackwell), Qwen3.8-Flash-Next IQ3_S, DFlash2
  `--spec 6 --draft-conf 0.35` against the MTP: **+7.8% on 19 short prompts, +1.1% on long thinking, faster on
  21 of 23 prompts**; answers identical on the short prompts (`drafting.py` comment, `EXPLAIN_NUMBERS`).
- On R9700 (gfx1201, p620), gfx12 Q4_0 GEMM variants cut drafter kernel time (gate+up 56→44.5 us, down 39→23.4 us,
  qkv 26.8→21.4, o 23.6→14.9, fc 61.5→35.2) and the serve A/B was +1.0% tok/s over the old variant, greedy ids
  identical (`b353ce7d`).
- Confidence steps (measured with DFlash2 alone against the MTP at equal VRAM, l620 mimic, IQ3_S): at 1,383 engine
  slots -4.2%/+3.1%/+7.1%/+9.3%/+8.7% for 0.35/0.5/0.65/0.8/0.9; the steps choose 0.8 below 3,000 slots, 0.7 from
  3,000, 0.5 from 7,500 and 0.35 from 12,000 (`drafting.py` `CONF_STEPS`, `6140ce1f`). A 16 GB RX 9070 XT with
  Flash-Next IQ3_S holds few thousand slots, so it will get 0.7 or 0.8, i.e. the "short prompt" end of the table.

**Verdict: worth enabling on the Flash-Next IQ3_S/Q2_0 sets.** Expected a few percent decode on short/chat
prompts and about a percent on long chains, plus ~0.2-0.37 GiB more expert cache than the MTP. Cost: one 1.2 GB
download + 0.52 GB on disk, one request at a time (the server enforces it), and no DFlash2 with `--parallel` > 1.
Keep the MTP if the PC is mostly used with several slots at once. Do not expect the +7.8% Blackwell number on this
card; the gfx1201 number is +7.2% short / +1.35% long.

---

## D. PLE on a 30 GiB PC: the auto policy will NOT trigger

- The policy is explicit: admit the 26.8 GiB table only when RAM holds `table + the expert arena + 8 GiB`, judged
  against what is available before either is allocated (`include/strata/core/ple_residency.hpp`,
  `ple_resident_admit`; the 0.2.7 rule was table + 8 GiB and is what broke a 64 GB Windows PC with error 1453).
- The table for IQ3_S is 26.8 GiB (`src/core/ple_residency_test.cpp`: "IQ3_S's table, 26.8 GiB"; 320,001,536 rows
  of 90 B per `docs/AMD_HIP.md`). The Flash-Next expert arena on this PC cannot be smaller than what the engine
  streams when the card holds ~5,000 slots; the full layout is ~46.8 GiB. Table + arena + 8 GiB is far above
  30 GiB usable.
- **Consequence:** `STRATA_PLE_AUTO_RAM` declines and the table is read direct from the SSD, exactly as on 0.2.6.
  Do not try `--ple-io ram` (it needs the RAM); that is why setup writes plain direct I/O for this class of PC.
- **Did 0.2.8-0.2.10 change PLE handling for small-RAM PCs?** No speed change; only safety: the admission rule
  above (`472de9cd`), giving the lock back when it starves the automatic conversation cache (`587a4744`), a better
  memlock message (`5a13c34f`), giving the lock back before the arena is registered (`eb891724`), the 20 s wait
  (`07d5ecfa`), and the "read experts from the file like `--mmap-experts`" fallback when RAM is short (`067b7433`).
  There is **no PLE row cache** in the tree (I searched; none exists), and no new I/O-thread tuning in this range.
  `STRATA_IO_THREADS`, `STRATA_STAGER_THREADS`, `STRATA_PREFILL_WARM`, `STRATA_PREFILL_STREAM_MIN` are older
  settings, unchanged here (they are described in `docs/AMD_HIP_PERFORMANCE.md` and `docs/DETAILS.md`).
- **One old opt-in is worth one A/B on this PC:** `--ple-io mmap`. On the R9700 the same docs give direct 2,045,
  mmap 2,276 (+11%) and RAM 2,506 (+22.5%) tok/s on an 8K prompt. The automatic mmap switch is off in 0.2.7
  (`STRATA_PLE_AUTO_MMAP=1` brings it back), so if prompt speed matters, try `--ple-io mmap` by hand. That is an
  existing option, not a merge change.
- For the record: `STRATA_PLE_COMPACT=1` and `STRATA_PLE_COPY_STREAM=1` (upload raw IQ4_NL rows, dequantize on
  the device) are the DeepSeek/prefill PLE levers from #3779; they need the table held in RAM (`--ple-io ram` or
  mmap), which this PC cannot do.

---

## E. Tiel / Cyber-Tiel (Qwen3.6-35B-A3B) and Q3_K: the fork's `STRATA_Q36_SKIP_MTP=1` is still needed

- **Does 0.2.10 take Q3_K experts in the qwen3.6 engine or its MTP block?** No. Upstream in this range touched
  `src/qwen35` three times, and none of them adds an expert format: `31c3a16f` (penalties), `dd63f88f` (warm-up KV
  bounds fix), `6c01f85a` (penalty index bounds). `git grep Q3_K ffa80d73 -- src/qwen35` is empty. The RDNA4
  kernel port `1f8ca8de` lands in the shared native kernels (`s2_expert_grouped.cu`, `fused_gr.cu`,
  `shared_expert.cu`, `mtp.cpp`, `verify_kernels.cu`), not in the `src/qwen35` engine. So the fork's setting
  (`STRATA_Q36_SKIP_MTP=1`, still present in the merged `src/qwen35/q36_engine.cu` at line ~1716) remains the way
  a Tiel-Coder file whose blk.40 MTP block is Q3_K loads. Without it the load still stops. `--mtp` is refused
  with it, so no MTP drafts on those files.
- **Any 0.2.7-0.2.10 Qwen3.6 engine improvement that changes hybrid (CPU-expert) speed?** None that I can find.
  The three commits above are not speed. `fit_start` skips an unmeasured family, so it leaves a Qwen3.6 start
  alone. The PCIe 0.15 default and `STRATA_ROUTE_TAIL_SKIP` belong to the native/QSA expert source
  (`src/core/expert_source.cpp`), not to the qwen3.6 engine's own CPU expert streaming. The expert-cache reserve
  fix `cfa197e2` makes the cache ~100 slots smaller. So the Oct 28 numbers for Tiel (47.69/26.60 tok/s) should
  not move because of the merge. **Inference, not measurement** - the repo has no Strata Tiel benchmark; the
  measured Tiel rows in `docs/MODEL_RESULTS.md` are llama.cpp Vulkan (29.78 tok/s decode at 260K, 41.33 at 8K on
  this PC). Confirm with a same-day A/B on the merged build before claiming any change.
- **What might improve Tiel's hybrid speed on this PC is orthogonal to upstream:** the fork's own
  `STRATA_Q36_LEAN_ARENA`/`STRATA_Q36_EXPERT_RESERVE_MIB` settings (`docs/QWEN36.md`), plus fit/planning. The
  upstream `ee455af7` setup KV-streaming change and the DFlash2/MTP VRAM releases do not apply to the qwen3.6
  engine the same way (its KV is per-slot 45 KB/token; the Flash-Next QSA semantics differ). Keep Tiel on
  llama.cpp Vulkan until someone measures Strata's Tiel on this PC.

---

## F. fit_start for a discrete AMD card (new in 0.2.10)

**What it does.** `strata_control/vram_fit.py` prices a start from a measured Flash-Next base and, when short of
VRAM, turns the backend's knobs. It never changes the context and never changes the number of requests at once
(owner rule, 2026-11-02); it moves work off the GPU instead. It is called from setup, `strata launch` and
`serve/server.py` on every engine start (`fit_start`), so app and Terminal launches get it automatically.

**Rules for this card (HIP):**
- Vendor `hip`, KNOBS = `("kv_resident",)` only. It walks `--kv-resident` down 20,480 → 8,192 → 4,096 (never
  below 4,096; the engine raises smaller values to 4,096). A value the person set (`user_args`) is kept.
- `BASE_SLOTTED_MIB[("hip","flash-next")] = 3,900 MiB`, measured on an R9700 (gfx1201, p620) squeezed by a
  memory hog: free VRAM before the expert cache is free minus 5,045 MiB, i.e. about 1.28 GiB less than CUDA.
  The commit gives the start walk: 7,942 MiB→787 slots 48-52 tok/s; 7,000→439; 6,400→273; 5,800 head moved to
  the host (31 tok/s); 5,200 and 4,600 refused with a plain line (`076a195b`).
- Skipped: a layer split, DeepSeek, Metal, an APU, and any model whose family has no measured base. A single
  request is priced on CUDA and HIP only (`SINGLE_MEASURED_VENDORS = ("cuda","hip")`); the model families with
  bases are Flash-Next (`("hip","flash-next")`) and Gyro-m/Gyro-s.
- **Practical consequence on this PC:** with 16 GB, 128K context, int8 KV and `--kv-resident 32768`, the estimate
  has ~9 GiB left for the expert cache, so a normal Flash-Next start is unchanged. `fit_start` only engages when
  free VRAM drops near ~5-6 GiB (a big desktop, another program, or a much bigger context). For Qwen3.6-35B-A3B
  the family is unmeasured, so `fit_start` leaves that start untouched. The Qwen dense models are likewise
  outside the measured families.
- Also new in 0.2.10 and part of the same plan: request slots are priced per session (`2a104287`, corrected by
  `e4be9e50`), a streamed session keeps 1,400 B/cell beyond `--kv-resident` (`8cb6e82d`), and setup streams the
  KV from 64K up (`ee455af7`, numbers in row 11).

---

## G. Dense 27B path (Saluki/Swift on the fork's dense engine)

The fork's dense engine is the upstream native dense engine plus the fork's own kernels/commits (the merge diff
`ffa80d73..HEAD` touches `dequant_bf16.cu`, `iq_kernels.cu`, `generate.cpp`, `src/qwen35/*`; it does **not** touch
`src/core/native_dense.cpp`). Upstream changes that reach that engine:

- **gfx12 decode switches and the gfx12 extra table (rows 7).** Measured on an R9700 IQ3_S pack at 8K: +3.5%
  decode, md5-identical answers. The dense engine reads these verify/attention kernels, so the exact part
  transfers. No separate dense measurement was published.
- **RDNA4 verify/draft/shared-expert/head kernels (row 4, commit `1f8ca8de`).** The commit says the S2 and head
  changes were run on gfx1201 only; it lists parity tests but no speed number. Static: expected small decode and
  draft-layer gains; unquantified in the repo.
- **MTP improvements:** the draft layer's q8_1 images on gfx12 (`db1bfaaa`), the T=5 hyper-connection read
  (`7d4feb2e`, 15.5 vs 16.2 us, bitwise), `--mtp-draft-vocab` (+2.7-3.1% English decode, release notes), and the
  head/draft write paths from `1f8ca8de`. All apply to an MTP run; the measurements are on RDNA4/R9700 or their
  own branches.
- **hipBLASLt for gfx1201 (row 15).** This is the one lever that could change dense prompt speed a lot, if the
  merged build links ROCm 7.2's hipBLASLt 1.2.2. Upstream docs: a 1.2.2 table "roughly doubled prompt speed" on
  an RDNA4 card relative to the plain path; the shipped `gfx1201-hipblaslt-100202.txt` measured 32K prompt
  638→1,177 tok/s and 7K 656→1,164 on a R9700 with a full IQ3_XXS set. A hand-built engine has no table unless
  pointed at it (`STRATA_HIPBLASLT_TUNING`), so check the merge build's env/`tuning/` folder. Measured on R9700,
  not on the 9070 XT; `docs/AMD_HIP.md` says a table calibrated for the 9070 XT was within noise end to end, so
  treat it as "try and A/B", not a promise.
- **Not upstream:** the fork's dense-specific work (IQ1_S/Q2_K decode kernels, sampled MTP drafting, conversation
  checkpoints, matmul-core Qwen35 attention) is ours; the triage does not credit it here.
- **KV options for dense:** `--kv q4_0` did not reproduce a gain on a 12 GB card (release notes 0.2.7); the
  history table has an old 0.1.8 row (+4% at 128K, perplexity +8-12%) and a 0.1.25 `--kv k8v4` row (+14 tok/s on
  an RTX 3090 at 198K, prompts 2-5% slower). Nothing in this merge adds a KV format. KVA (row 14) is the only
  long-prompt trade, and it is for Flash-Next only.
- **Prompt cache / message-boundary cache:** not changed in this range; those are fork/existing features
  (`docs/MESSAGE_BOUNDARY_CACHE.md`, `docs/PROMPT_CACHE_TAIL.md`).

---

## H. Qwen3.8-Flash-Next prefill specifically (prompt 363-672 tok/s on this PC vs ~2,500 on an R9700)

The gap is the card's size and the host path, so the levers that matter are the ones that cut host traffic or
use the idle CPU pool. In this merge:

1. **Fused IQ2_S/IQ4_XS prompt experts now default ON (row 1).** The IQ3_S pack's 21 layers that used to keep MMQ
   now run the fused int8 kernels. Measured on R9700: +10.8% at 2K, +12.2% at 4K, +13.2% at 8K, +10.0% at 32K
   prompts, 5/5 pairs, 1K/1.5K unchanged. On a 16 GB card these are prompt-path kernel replacements; the same
   direction should apply, but no 9070 XT number exists. Quality caveat is real (2 of 19 quick-suite tasks lost
   when first measured; the default was then taken on the owner's decision - re-check the tasks the user cares
   about).
2. **CPU prefill share default on one gfx12 card (row 2):** R9700 +18.3% at 500, +30.6% at 1K prompts, 2K/8K
   unchanged, KL 0.022 at 1K top-1 29/29. Only for chunks under the share limit (1,536 tokens now that the IQ2_S
   fused default is on). For a 16 GB card that streams experts this is the classic TTFT lever for short prompts;
   long prompts (>3K) keep the old path.
3. **QSA prompt kernels (row 10).** The gfx12 matrix-core prompt attention gets the TRIM/RPF exact changes (gfx1151
   kernel numbers above; gfx1201 unmeasured) and the QSA norm+rope and gate fusions, which are exact and cut
   ~0.7 GB of traffic per QSA layer at 8K. That is host/VRAM traffic, exactly the kind of thing that matters on a
   small card, but upstream has no whole-model number.
4. **GDN chunked via the server (row 8):** +3.5% at 4K / +5.5% at 16K prompts on an R9700, KL 0.0025.
5. **More expert slots = faster hybrid prompts (row 11, `ee455af7`):** on a 12 GB mimic IQ3_S the same engine read
   a 32K prompt in 12-17 s instead of 21-34 s and a 118K prompt in 45-63 s instead of 76-122 s. On a 16 GB
   9070 XT with 128K context, check the current config: if the run config pins `--kv-resident` lower or setup
   predates the 64K-up rule, re-run setup/`strata launch` so the KV streams and the experts get the room.
6. **`STRATA_PREFILL_CPU_SHARE_BATCH=1` and `STRATA_PREFILL_ROUTED_STREAM=1` (both new-in-range opt-ins):**
   the first gives the CPU share to a read with no slot decoding beside `--batch` slots (`3b3f3b6b`/`1b1a28e7`);
   the second drops streamed experts no token routes to and caps copies queued ahead of the link
   (`5273fb66`, header says single GPU; upstream measured on an RTX 3090, so HIP is static). Both are
   experiments; try one at a time and compare same-day.
7. **What is *not* here:** no new I/O-thread tuning, no chunk-size change on gfx12 for this merge (the 16,384
   auto chunk predates it), no PLE speed lever for this PC (section D), and the layer-major prefill is off on AMD
   (`d02478b7`) with only an unqualified HIP opt-in (`STRATA_LM_HIP=1`). On a single card upstream explicitly
   keeps the chunk-major path.

**What I would try first on this PC, in order:** (1) make sure the build has `STRATA_PREFILL_MMQ=ON` and
`-DCMAKE_HIP_ARCHITECTURES=gfx1201`; (2) confirm the fused IQ2_S default is active (look for the engine's fused
line); (3) `--ple-io mmap` A/B (old option); (4) DFlash2 for Flash-Next; (5) the two 0.2.10 opt-ins above one at
a time; (6) `STRATA_ROUTE_TAIL_SKIP` is already on. Keep the spec/chunk/context fixed between arms, and use the
fork's own benchmark scripts. Numbers above are from an R9700 or an RTX 5070 and are not predictions for this
card.

---

## Enable these on the 9070 XT (exact flags/env, expected effect)

Defaults are already what we want for most of it; this is the short list of what to set or check, not a list of
new flags to add everywhere.

1. **Build:** HIP with `-DCMAKE_HIP_ARCHITECTURES=gfx1201 -DSTRATA_PREFILL_MMQ=ON -DSTRATA_NATIVE_EXPERTS=ON`.
   Without MMQ the fused prompt experts (rows 1-2, the biggest prompt lever) are not compiled. Lead: verify the
   local cache has them.
2. **Leave the gfx12 defaults alone** (they are on): `STRATA_PF_FUSED` (unset), `STRATA_PF_FUSED_IQ2S` (unset),
   `STRATA_HIP_WMMA` (unset), `STRATA_SELECT_WMMA` (unset), `STRATA_HC_Q8` (unset), the four decode switches
   (unset), `STRATA_HEAD_MIX_MULTI` (unset), `STRATA_SCORES_THREAD` (unset), `STRATA_HIP_QUEUE_PRIME` (unset).
   Do **not** set `STRATA_DEFAULTS=026` or `STRATA_GFX12_DEFAULTS=0` for speed.
3. **Do not touch `--pcie-frac`**: the gfx12 default is 0.15 (measured +9.9%/+25.6% decode on R9700); only an
   explicit value the person set wins.
4. **`--ple-io`**: leave the auto policy alone. A/B `--ple-io mmap` on this PC; never `--ple-io ram` (26.8 GiB
   table, 30 GiB RAM).
5. **Flash-Next:** DFlash2 `auto` is selected for the ISTA-DAS IQ3_S/Q2_0 sets on HIP gfx1201. If the drafter
   pack is not on disk, fetch it once (1.2 GB download, 0.52 GB packed, 715 MiB in VRAM with the converted
   selector vs 1,082 for MTP) or set `--drafting mtp` to keep the old behaviour. Do not combine DFlash2 with
   `--parallel` > 1.
6. **MTP runs (dense 27B, Coder, Tiel):** `--mtp-draft-vocab <file>` (+2.7-3.1% English decode, upstream number)
   if the file exists. Keep `--spec` as tuned (3/4); nothing in 0.2.10 asks for a bigger window on one card.
7. **Tiel-Coder / Cyber-Tiel:** `STRATA_Q36_SKIP_MTP=1` + `--mtp 0` (config `args`) remains required for a Q3_K
   MTP block; nothing upstream removes it.
8. **hipBLASLt:** if the merged build loads shim/system hipBLASLt 1.2.2, point `STRATA_HIPBLASLT_TUNING` at
   `tools/hip/gfx1201-hipblaslt-100202.txt` and A/B prompt speed. If the build bundles another version, do not.
9. **Watch the new fallbacks:** `--ram-keep-gib` only if the user wants RAM held for other work; the arena
   fallback is automatic. `STRATA_ARENA_RAM_FALLBACK=0` restores old behaviour for an A/B.
10. **Configs written by an older setup:** re-run setup/`strata launch` (or check) so the 64K-up KV streaming rule
    (`ee455af7`) and fit_start apply. Hand-written configs keep what they have.

## Not for us (and why)

- **Multi-card split engine (0.2.10 headline, `202bb94a`, `28f94d68`, `ea2b0d9c`...).** Needs 2-4 cards; the RX
  9070 XT is a single card. Also Terminal-only now.
- **Layer-major prefill (0.2.7, `ffe40c16`, `c683fbf0`...).** Stays off on AMD builds (`d02478b7`); upstream's
  numbers are two RTX A4000 cards. `STRATA_LM_HIP=1` exists but is unqualified on RDNA4.
- **Route prior / `STRATA_ROUTE_PRIOR` for 12 GB NVIDIA.** Reverted to opt-in in 0.2.8 and off in HIP builds
  (`7c4c2079`, `20a63cf0`); it is NVIDIA-only.
- **RTX 40 mmvq tables, sm_80/A100 defaults, sm_120/sm_121 (DGX Spark) defaults, PDL, GR_V3.** CUDA-only families;
  the engine does not read them on HIP.
- **MTP batch/draft features tied to CUDA graphs or a layer split:** `--lookup-max-t` (CUDA one-GPU 8-row build),
  `STRATA_MTP_ROOM_MIN` (CUDA), `STRATA_PIPED_MTP` (two-card split), `--pipeline-windows` (two-card).
- **Strix Halo (gfx1151) and Ryzen AI Max work, Windows-specific work, Mac/Metal, Intel Arc/SYCL, DGX Spark,
  Blackwell DeepSeek vision tower, DeepSeek-only RAM-store/prompt changes.** Different hardware or engine.
- **DeepSeek RDNA4 defaults** (`3b64abf5`, `164619c5`, `386d2e44` five-experts default, `DS4_FAST_ATTN`): this
  PC does not run DeepSeek-V4.1; its models are Qwen families.
- **`--lookup-max-t 12` default (0.2.10):** CUDA only, not HIP (`docs/DETAILS.md`).
- **KVA as a default:** it ships off and costs 11% decode on a fast GPU per `docs/KVA.md`; only worth a try for
  very long prompts, with the quality caveat.
- **Picture-reader changes on AMD (`e5de7428`):** a capability item (the Qwen on-demand reader now works on HIP but
  is off by default); not a speed item, and the 16 GB card uses the CPU reader anyway per `docs/QWEN36.md`.

## What is not measured, and what I could not verify

- No repo measurement exists for **any of these changes on an RX 9070 XT 16 GB specifically** except where the
  source says "gfx1201"/"R9700" (the R9700 is gfx1201 too) or the branch notes "run on gfx1201 only". The 9070 XT
  rows in `docs/AMD_HIP.md` are an older 0.1.30 engine.
- **Static only:** the RDNA4 kernel port's size (`1f8ca8de`), the gfx12 extra decode table (`09982fff`), the QSA
  TRIM/RPF numbers applied to gfx1201 (`2970fe74` was gfx1151), `STRATA_IL_PAD` (`4c5a5549`), the streamed-session
  number (`8cb6e82d` measured on CUDA), `STRATA_PREFILL_ROUTED_STREAM` on HIP, and the effect of fit_start on this
  exact 16 GB start (my arithmetic from the measured HIP base).
- **Unverified/untested by upstream:** the arena RAM-copy fallback's first patch (`43543e9b`: "not built, not
  run: the patch is untested"); its speed number is from an RTX 5070.
- **Quality caveats are real:** fused IQ2_S loses 2/19 quick-suite tasks in its own measurement; SELECT_WMMA
  changes values above ~28K and the PCIe 0.15 default changes bits slightly (KL 0.0013); GDN chunked KL 0.0025,
  suite 55 vs 56. Run the user's own quality checks before switching a workflow (agent loops especially).
- I did not find 0.2.8/0.2.9/0.2.10 release-note files in the repository; their changes were read from commits
  and docs only. The fork's dense-engine commits are not part of this triage.

## Citations

Git (hashes are in the table above; full messages can be read with `git show <hash>`): `4b9e05a2` (0.2.6 base),
`ffa80d73` (upstream 0.2.10), `5eab60ce` (merge). Key paths: `src/core/arch_defaults.cpp`,
`include/strata/core/arch_defaults.hpp`, `include/strata/core/gfx12_extra.hpp`,
`include/strata/core/gfx12_platform_defaults.hpp`, `include/strata/core/defaults_026.hpp`,
`include/strata/core/ple_residency.hpp`, `include/strata/prefill/share_rules.hpp`,
`include/strata/prefill/moe_fused_iq.hpp`, `src/prefill/moe_fused_iq.cu` (and the other files named in the table),
`serve/server.py` (`GDN_STACK_GFX12`), `strata_control/drafting.py`, `strata_control/vram_fit.py`,
`include/strata/kernels/window_limits.hpp`, `docs/BATCHING.md`, `docs/KVA.md`, `docs/QWEN36.md`,
`docs/AMD_HIP.md`, `docs/DETAILS.md`, `docs/MODEL_RESULTS.md`,
`docs/CONVERSATION_CHECKPOINTS.md`, `docs/release-notes-0.2.7.md`.

Repo docs by section used above: `docs/AMD_HIP.md` "Speed switches", "RDNA4 (gfx1201)", "Tuning table";
`docs/DETAILS.md` "On by default", "Opt-in switches", "History / older measurements"; `docs/BATCHING.md`
"Windows of 16 rows"; `docs/KVA.md`; `docs/QWEN36.md` "Speed and exactness", "History"; `docs/MODEL_RESULTS.md`
(the RX 9070 XT tables); `docs/release-notes-0.2.7.md`.

## Verification of the git state

- `HEAD 5eab60ce` is the merge of the fork branch and upstream `ffa80d73`; `git merge-base 4b9e05a2 ffa80d73`
  is `4b9e05a2`, and `git rev-list --count 4b9e05a2..ffa80d73` is 1,441.
- The fork side does not modify the gfx12 default files (`git diff --name-only ffa80d73 HEAD` has no
  `src/core/arch_defaults.cpp`, no `include/strata/core/*`), so the defaults above are upstream's after the merge.
- `docs/work/` is untouched by this task except this file.
