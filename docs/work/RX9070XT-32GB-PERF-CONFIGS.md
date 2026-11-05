# RX 9070 XT 16 GB + 32 GB RAM: fastest configs for the installed Qwen models

This is the decision document for one PC: AMD Radeon RX 9070 XT 16 GB (gfx1201, 16,304 MiB; the desktop holds
1,115-2,026 MiB of it), Ryzen 7 7800X3D (8 cores/16 threads), 32 GB RAM (31 GiB usable), ROCm 7.2.4 on Linux.
It is documentation and arithmetic only: **no model files are installed on this PC right now and no engine was run
on a model for this document**, so every number below is either quoted from the tree's measured tables (with its
hardware and build) or marked *arithmetic*. Nothing here was measured by the author.

Flag names were checked against the installed engine (`…/0.1.56-linux-x86_64-hip-rx9070/strata --help`, read-only) and
`BUILD.json`; the `strata` CLI itself is **not installed on this PC** (no `strata` on PATH), so `strata launch`
commands below are the documented Terminal form (docs/TERMINAL_LAUNCH.md) and were not run here. The qwen36 engine's
flags (`--expert-vram`, `--expert-slots`, `--hybrid-rows`, `--cpu-threads`, `--ctx`) are not in that `--help` page —
they are taken from `src/qwen35/q36_engine.cu`.

The two engines on this PC are different products, and the split matters for every recommendation:

| Engine | Binary / build | Runs | hipBLASLt |
| --- | --- | --- | --- |
| Installed app engine **0.1.56** | `~/.local/share/strata/components/engine/0.1.56-linux-x86_64-hip-rx9070/strata` (`BUILD.json`: `f-amd-vision c75ab7655503`, gfx1200+gfx1201) | Qwen3.8-Flash-Next (ISTA-DAS, Coder, Gyro), Qwen3.6-35B-A3B (`qwen35moe`) | **bundled 1.3.0 (100300)** — the gfx1201 `100202` tables do **not** load |
| Fork dense engine | source build of this branch: `build-hip/strata` (docs/CONVERSATION_CHECKPOINTS.md, profiles use `@bundle/dense-hip`) | Saluki / Swift **Qwen3.8-27B dense** (`qwen35` dense, `STRATA_Q35_DENSE=1`, fork-only; upstream engine rejects it) | system ROCm 7.2.4 = hipBLASLt **1.2.2 (100202)** — `tools/hip/gfx1201-*-100202.txt` apply |
| Fork native router | `strata-native` + the router profiles in `~/.config/strata/` (opt-in, Linux only) | wraps the two engines above and bundled llama.cpp (Vulkan/ROCm) | passes the profile's env to the engine |

Sources read: [docs/MODEL_RESULTS.md](../MODEL_RESULTS.md), [docs/QWEN36.md](../QWEN36.md),
[docs/MODELS.md](../MODELS.md), [docs/QUANTS.md](../QUANTS.md), [docs/DETAILS.md](../DETAILS.md),
[docs/UNSLOTH_Q4.md](../UNSLOTH_Q4.md), [docs/CONVERSATION_CHECKPOINTS.md](../CONVERSATION_CHECKPOINTS.md),
[docs/AMD_HIP.md](../AMD_HIP.md), [docs/release-notes-0.2.7.md](../release-notes-0.2.7.md),
[docs/TERMINAL_LAUNCH.md](../TERMINAL_LAUNCH.md), the fork commit `650737c9` message (Tiel native measurements),
the installed engine's `--help` / `BUILD.json`, `~/.strata/data/native-tune/*.json`, `~/.strata/data/state.json`,
`~/.config/strata/{router,native-tuning-router}.json`, `~/.config/strata/profiles/unified/*.json`, and the app engine
logs `~/.local/share/strata/install/strata-*.log`.

---

## 0. The answer first

Measured decodes are from the fork's RX 9070 XT tables (Oct 25-26, router AppImage `0ae6890f…` = `strata-native`
from `codex/rx9070xt-perf` `d2b0565b` on the 0.2.6 rebase; see MODEL_RESULTS) unless marked otherwise.
"tok/s" is decode unless it says prompt.

| Family | Fastest choice on this PC | Engine | Context | Expected (measured, this PC) |
| --- | --- | --- | --- | --- |
| Qwen3.8-27B dense | **Saluki 27B IQ2-mix + MTP** for speed | fork dense engine | 131,072 | 8K: 1,317 prompt / 70.8 decode; 128K: 762 / 45.7 (fork `d2b0565b`, Oct 26) |
| Qwen3.8-27B dense | **Swift 1.5 IQ3_XXS + MTP** speed/quality compromise | fork dense engine | 131,072 | 8K: 1,280-1,314 / 66.8-69.1; 128K: 731-755 / 15.9-17.9 (fork, Oct 25-26) |
| Qwen3.8-27B dense | Swift 1.5 **IQ3_S + MTP** for quality, short context only | fork dense engine | ≤32K (measured 8K) | 8K: 1,180-1,256 / 65.5-68.6; 128K: 740 / 11.7-12.7 (fork, Oct 25-26) — 4x slower at 128K |
| Qwen3.8-Flash-Next | **Coder IQ1_M** (code-only quality) | app engine 0.1.56 | 131,072, int8 KV | ~55 decode, ~530 fresh-prompt (app 0.1.56 log, this PC, 3 Nov, median of 59 requests; not a benchmark) |
| Qwen3.8-Flash-Next | **Q2_0** (general, fastest of the full-expert packs) | app/fork Flash engine | 131,072, int8 KV + `--kv-resident 32768` | 8K: 452 / 16.5; 128K: 672 / 19.8 (fork, Oct 26, 11 GiB RAM tier) |
| Qwen3.8-Flash-Next | IQ3_XXS; IQ3_S **not viable** on 32 GB | Flash engine | 131,072 | IQ3_XXS 8K: 363 / 11.6, 128K: 435 / 9.0 (fork, Oct 26). IQ3_S: no usable number, app 0.1.56 measured 0.8-8.3 tok/s |
| Qwen3.6-35B-A3B | **llama.cpp Vulkan, 26 CPU expert layers, q8_0 KV** | router llama.cpp | 262,144 | 8K: 1,928-1,942 / 40.4-41.3; 262K: 990 / 29.8-32.1 (fork, Oct 25-26) |
| Qwen3.6-35B-A3B | Strata qwen36 engine, prompts ≤16K only | fork dense/qwen36 engine | 16,384 (measured 8K) | 8K: 1,725 / 47.69; 32K prompt: 1,135 / 26.60 (fork `9699f5b4`, this PC, Oct 28; untimed, other load) — beats llama.cpp only at 8K |
| Other, fits 32 GB | Qwen3.6 UD-Q6_K_XL needs 44 GB RAM; Q8_0 52 GB: **do not**; Flash-Next Unsloth needs 72 GB+ RAM: **no**; DeepSeek needs 46 GB+: **no** | — | — | see §1.6 |

The TL;DR decision: **use the dense 27B (Saluki for speed) as the fast daily model; Coder IQ1_M when it must be
Flash-Next; Tiel through llama.cpp Vulkan for long context and through Strata's qwen36 engine only when prompts are
short (≤16K).** On this PC the 16 GB card plus 32 GB RAM is the wall for every model that centres on a 26.8 GiB PLE
table or 34-50 GB of experts; the 27B dense model is the only family whose whole working set lives in VRAM + RAM.

---

## 1. Model / quant selection, with memory math

### 1.1 The fixed numbers

- VRAM 16,304 MiB; desktop 1,115-2,026 MiB (MODEL_RESULTS "The PC", measured Oct 25-26). Plan on roughly 14-15 GiB
  of the card for the model, and read the engine's own free-VRAM line; the highest total peak in the measured tables
  was 15,816 MiB (Swift IQ3_S vision at 8K, Oct 25).
- RAM 31 GiB usable. The engine's memory guard on this PC reported `limit 30.9 GiB`, floor 2,560 MiB
  (`strata-*.log`, app 0.1.56, 3 Nov).
- The native server refuses/cancels a request when less than 768 MiB of VRAM is free (`--vram-floor-mib 512` + 256;
  frontend/main.cpp, QWEN36.md).
- PLE ("n-gram") table: 320,001,536 rows × 90 B = **26.8 GiB**, always read from the model shard unless it is in RAM
  (DETAILS.md; AMD_HIP.md). `--ple-io ram` needs the table plus 8 GiB spare (auto policy, 0.2.7+), i.e. ~35 GiB —
  **impossible on 30 GiB usable RAM**. Keep `--ple-io direct` (default); do not set `--ple-io ram`.
  - The RAM-sized knob is `--ple-row-cache N` (bounded row cache, 90 B/row, default 1,048,576 rows ≈ 90 MiB).
    Raising it costs RAM only; its speed effect on this PC is **not measured** (idea, not a claim).

### 1.2 KV cache per 1K cells (arithmetic from the tree's geometry; formats from the installed engine's `--help`)

Flash-Next has 12 QSA (full-attention) layers, 2 KV heads × head_dim 256 (`include/strata/kernels/qsa.hpp`,
`kv_q8.hpp`). The dense 27B has 64 layers with `full_attention_interval` 4 → 16 QSA layers
(`include/strata/core/geometry.hpp`; profiles set `STRATA_Q35_KV_I8=1`). Qwen3.6-35B-A3B has 10 QSA layers + the MTP
block (QWEN36.md: "45 KB per token"; 2 KV heads × 256 × 2 (K,V) × 4 B × 11 = 45,056 B/token).

Flash-Next (MoE) — bytes per cell = one token, one attention layer:

| `--kv` | B/cell/layer | B per token (12 layers) | per 1K tokens | 8K | 32K | 128K | 262K |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `fp16` | 2,048 | 24,576 | 24.0 MiB | 192 MiB | 768 MiB | 3.00 GiB | 6.00 GiB |
| `int8` (default) | 1,056 | 12,672 | 12.4 MiB | 99 MiB | 396 MiB | 1.55 GiB | 3.09 GiB |
| `k8v4` | 816 | 9,792 | 9.6 MiB | 77 MiB | 306 MiB | 1.20 GiB | 2.39 GiB |
| `q4_0` | 576 | 6,912 | 6.8 MiB | 54 MiB | 216 MiB | 0.84 GiB | 1.69 GiB |

Measured cross-check: the app engine on this PC logged `KV streaming: 32768 of 131072 cells … the K/V in 1.55 GiB of
pinned RAM` (`strata-gyro-gyro-s.log`, 0.1.56) — exactly the int8 row at 131,072. Add the FP32 pooled indexer keys,
which stay in VRAM and cost ~5 KiB/token (≈5 MiB/1K, 640 MiB at 128K, 1.28 GiB at 262K; `kv_q8.hpp` comment gives
29 KB/token with fp16 K/V).

Dense 27B — the fork engine supports int8, fp16 and fp32 only (no `--kv q4_0`/`k8v4` on this path;
`src/qwen35/q36_engine.cu` line 2111):

| KV | B per token (K/V; int8 row adds ~2 KiB indexer) | per 1K | 8K | 32K | 128K |
| --- | ---: | ---: | ---: | ---: | ---: |
| `STRATA_Q35_KV_I8=1` int8 | ~16.9 KiB + 2 KiB indexer | 18.5 MiB | 148 MiB | 578 MiB | 2.25 GiB |
| FP16 (`STRATA_Q35_KV_F16`) | 32 KiB | 32.0 MiB | 256 MiB | 1.00 GiB | 4.00 GiB |
| FP32 (neither set) | 64 KiB | 64.0 MiB | 512 MiB | 2.00 GiB | 8.00 GiB |

Qwen3.6-35B-A3B qwen36 engine (fp32 K/V in the session caches; **no** `--kv` option on this path):

| | B per token | per 1K | 8K | 16K | 32K | 40K | 128K | 262K |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| one session | 45,056 | 44.0 MiB | 352 MiB | 704 MiB | 1.38 GiB | 1.72 GiB | 5.50 GiB | 11.00 GiB |

Tiel through llama.cpp (from MODEL_RESULTS' own router-check arithmetic: 14,766 MiB needed with 25 CPU layers,
14,302 with 26, 13,838 with 27 at 262,144, file 21,696 MiB, 464 MiB experts/layer; the KV term is 2,622 MiB):

| llama.cpp KV | B/token | per 1K | 8K | 32K | 128K | 262K |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `q8_0` (deployed) | ~10,500 | ~10.2 MiB | 82 MiB | 328 MiB | 1.28 GiB | 2.56 GiB |
| `f16` | ~19,750 | ~19.3 MiB | 154 MiB | 617 MiB | 2.41 GiB | 4.82 GiB |

These llama.cpp numbers are **inferred** from MODEL_RESULTS' own router check (14,302 MiB needed with 26 CPU layers
at 262,144) rather than printed by the engine; the ratio between `f16` and `q8_0` is the one backend.hpp charges.

### 1.3 Qwen3.8-27B dense (Saluki / Swift 1.5): memory and quality

Weights in the GGUF: Saluki `Underdog-Saluki-27B-1.0-IQ2-mix-MTP-abliterated.gguf` **7.83 GiB**;
Swift `IQ3_S-MTP` **11.32 GiB**; Swift `IQ3_XXS-MTP` **9.73 GiB** (profile `auto_size`
10,442,827,776 B; the three sizes are CONVERSATION_CHECKPOINTS.md + profiles).

Arithmetic at the recommended context 131,072 with int8 KV, 1,536 MiB runtime reserve, and no checkpoints:

| Model | weights | int8 KV + indexer | reserve | desktop | total | outcome |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Saluki IQ2-mix | 7.83 | 2.27 | 1.50 | 1.1-1.6 | 12.7-13.2 GiB | fits comfortably (measured peak 14,758 MiB total) |
| Swift IQ3_XXS | 9.73 | 2.27 | 1.50 | 1.1-1.6 | 14.6-15.1 | borderline; mostly resident (measured peak 15,278) |
| Swift IQ3_S | 11.32 | 2.27 | 1.50 | 1.1-1.6 | 16.2-16.7 | over budget; the engine leaves ~2 GiB of weights file-backed at 128K (`STRATA_Q35_ADAPTIVE_WEIGHTS=1`), which is why 128K decode is 12 tok/s (peak 15,330) |

At 8K all three fit fully (decodes 65-72 tok/s). There is no measured 32K row for any dense profile (MODEL_RESULTS
"Not measured"), so 32K is *arithmetic only*: between the 8K and 128K rows, with the IQ3_S risk that the weight
spill starts somewhere between 8K and 128K.

Quality evidence for the dense 27B is thin in this tree: MODEL_RESULTS shows identical retrieval (3/3) and code
(13/13) checks for Saluki IQ2-mix and both Swift sizes; there is **no perplexity/KL table for the 27B family**
(the QUANTS.md table is Qwen3.8-Flash-Next). Saluki is an abliterated fine-tune (safety behaviour changed) and
Swift 1.5 "thinks much shorter before answering, with about the same quality" (MODELS.md). Treat IQ2-mix as the
fast, lower-fidelity choice; IQ3_S as the fidelity choice at short context; IQ3_XXS as the compromise.

Also measured, the same PC and app: llama.cpp Vulkan was slower than the fork dense engine for these models —
Saluki 33.91 tok/s at 128K/49.68 at 8K (Oct 16, separate server), Swift IQ3_XXS 4.95 at 128K/15.51 at 8K
(MODEL_RESULTS "Before / Now"). The fork engine is the fast path for the 27B.

### 1.4 Qwen3.8-Flash-Next: the RAM is the wall, not the card

MODELS.md: the model fits when RAM ≥ experts + ~10 GB. On 31 GiB usable that means **none** of the ISTA-DASLab
full-expert packs fits resident: Q2_0 34 GB experts, IQ3_XXS 43 GB, IQ3_S 50 GB. Only the Coder fits
(shard 1 29.6 GB, 23 GB of experts). So the full-expert packs run in the engine's mapped/RAM-budget mode: the fork's
measured profiles use **`--resident-budget-gib 11`** (11 GiB of the hottest experts in RAM, the rest read from the
NVMe GGUF per token) with `--kv-resident 32768` (K/V in pinned RAM, 1.55 GiB at 131K) and a 3,072-3,584 MiB VRAM
reserve. Measured least RAM available in those runs: 6.2-8.0 GiB (MODEL_RESULTS).

Memory math at 131,072 / int8 KV (arithmetic, using the tree's numbers):

| Pack | weights (dense side + experts the card doesn't hold) | RAM tier | VRAM at 128K (dense ~4.8 GB + KV VRAM 0.42 + indexer 0.64 + expert cache + reserve) | comment |
| --- | ---: | ---: | --- | --- |
| Q2_0 | experts 34 GB; dense ~4.8 GB | 11 GiB | measured peak 14,558 MiB total | 1.55 GiB K/V pinned RAM + 90 MiB PLE rows |
| IQ3_XXS | experts 43 GB; dense ~4.8 GB | 11 GiB | measured peak 14,808 MiB total | more SSD reads per token than Q2_0 |
| IQ3_S | experts 50 GB; dense ~4.8 GB | 11 GiB would leave ~39 GB from SSD | not measured; app 0.1.56 with `--resident-experts` ran at 0.8-8.3 tok/s | **not a speed path on 32 GB** |
| Coder IQ1_M | 23 GB experts, shard 1 29.6 GB | resident mode: 18.93 GiB measured, 0 file reads | 1,640 MiB free after load, expert cache 1.43 GiB | the one full-speed Flash-Next here |
| Gyro-S | 58.5 GB download, ~27.6 GB overall memory | — | app 0.1.56 run: 2.1 tok/s | memory fallback only; slower and lower quality |

The PLE table (26.8 GiB) cannot live in RAM, so prompts are SSD-read bound: the fork measured 363-672 tok/s prompt
speed at 8K-128K for Q2_0/IQ3_XXS (MODEL_RESULTS). That is the single biggest reason Flash-Next is not the fast
family here.

Quality (QUANTS.md, 4,995 teacher-forced positions on an R9700, against Q8_0):

| Pack | same top token | mean KL | perplexity (Q8_0 4.052) |
| --- | ---: | ---: | ---: |
| IQ3_S | 87.69% | 0.144 | 4.097 |
| IQ3_XXS | 84.46% | 0.202 | 4.119 |
| Q2_0 | 80.60% | 0.315 | 4.475 |

Coder is expert-pruned: ISTA-DASLab reports 91% of the full model's SWE-bench Verified and 99% of LiveCodeBench,
and it is weaker outside code and in non-English/CJK text (MODELS.md). Gyro-S is the smallest and lowest-quality
option and is slower; do not promote it as a fast path.

### 1.5 Qwen3.6-35B-A3B (Tiel-Coder / Cyber-Tiel UD-Q4_K_XL)

File 21,696 MiB (21.2 GiB), experts 464 MiB in most layers, MTP block `blk.40` is Q3_K — the native expert kernels
do not take Q3_K, so the qwen36 engine refuses to load unless `STRATA_Q36_SKIP_MTP=1` (commit `650737c9`;
QWEN36.md). With that setting there are **no MTP drafts**; `--mtp` is refused. Both Tiel and Cyber Tiel have the Q3_K
MTP block (commit `650737c9`, limits section).

Three paths:

1. **llama.cpp Vulkan, 26 CPU expert layers, q8_0 KV, 262,144 context** (deployed, measured).
   VRAM: 21,696 − 26×464 = 9,632 MiB of weights, + 2,622 MiB q8_0 KV, + 2 GiB headroom = **14,302 MiB**, leaving
   the desktop up to 2,002 MiB of the card (MODEL_RESULTS "Tiel's CPU expert layers"). Measured peak 14,231-14,289 MiB.
   At 8K the KV shrinks by 2.5 GiB; 27 CPU layers would fit (13,838 needed) but the profile keeps 26 for the
   262K context. The profile puts llama.cpp on **Vulkan** (`inference.device: Vulkan0`); the router's ROCm llama.cpp
   build is the other option, and no ROCm-vs-Vulkan comparison for Tiel is documented in this tree.
2. **Strata qwen36 engine (hybrid CPU experts)** — measured on this PC, Oct 28 (commit `650737c9` block C):
   `STRATA_Q36_SKIP_MTP=1 STRATA_Q36_LEAN_ARENA=1 STRATA_Q36_EXPERT_RESERVE_MIB=1536`, 16,384 context:
   8K prompt 1,725 tok/s, decode **47.69** tok/s, 93/256 experts/layer in VRAM, 11.67 GiB pinned host RAM,
   least free VRAM 1,442 MiB, least RAM 8.0 GiB. 40,960 context: 32K prompt 1,135 tok/s, decode **26.60** tok/s,
   78/256 experts/layer, 12.74 GiB pinned host, least free VRAM 1,292 MiB, least RAM 8.8 GiB.
   The qwen36 path has **no `--kv` option**: K/V are fp32, 45 KiB/token (table in §1.2), which is why 262,144 does not
   fit on 16 GB — 11.0 GiB of KV + ~3 GiB of dense weights + reserve leaves no expert slots (arithmetic). Use
   `--max-context 16384` (measured) or 40,960 (measured); above that llama.cpp wins.
   `--resident-cpu-experts` is **not** a qwen36 flag (it belongs to the Flash-Next/dense engine and needs
   `--mmap-experts` + a static `--expert-profile`); the qwen36 hybrid mode is automatic, sized by `--expert-vram`
   (GiB) / `--expert-slots` if you want to cap it.
3. **Upstream app qwen36 path** (installed app 0.1.56): the same model family, engine 0.1.56.
   No measurement exists on this 16 GB card; the nearest small-card measurement is an RTX 3060 12 GB hybrid at
   59.7 tok/s (QWEN36.md), and the R9700 32 GB all-in-VRAM figure is 117.8 tok/s (`--mtp 2` 183.7 in the 0.1.45
   speed work). Those are **different hardware**; do not transfer them to this card.

Measured comparison (same PC): llama.cpp 32K decode 38.22/38.59 tok/s vs Strata native 26.60 (Oct 28); at 8K
Strata 47.69 vs llama.cpp 41.33 (Oct 26, sampled, another day). So: **Strata native for short prompts, llama.cpp
for long ones**. Quality: the tree has no quality benchmark for Tiel-Coder/Cyber-Tiel; MODEL_RESULTS' code checks
are 10-12/13 (Tiel) and 12-13/13 (Cyber Tiel), retrieval 3/3. Treat quality as unverified.

### 1.6 Other models that fit (or don't)

- **Qwen3.6-35B-A3B UD-Q6_K_XL** (32.6 GB, setup plans 44 GB RAM) and **Q8_0** (37.8 GB, 52 GB): do not fit 31 GiB
  RAM alongside the OS/engine; not recommended on this PC.
- **Qwen3.6-35B-A3B UD-Q4_K_XL non-Tiel** (stock Unsloth): same 22.9 GB file class; if its MTP block loads, the
  qwen36 engine can draft (~1.6x decode on cards that hold the experts; QWEN36.md history) — unverified here.
- **Flash-Next Unsloth UD-IQ4_XS / UD-Q4_K_XL** need ~72-88 GB RAM for all experts; on 32 GB they stream from SSD and
  are much slower (UNSLOTH_Q4.md). **No.**
- **DeepSeek V4.1-Flash** needs ≥46 GB RAM and 239-274 GB disk. **No.**
- **A dense 27B at a higher quant** (e.g. IQ4_XS/Q4_K_M) is not documented in this tree and has no measured data on
  this card. Its weights plus KV would be roughly 16-18 GB at short context (arithmetic), i.e. it would not fit the
  16 GB card resident and would stream from RAM/SSD; recommending it would be unmeasured. The three documented dense
  packs above are the only evidence-backed choice.
- **Gyro-S** (58.5 GB download, 27.6 GB memory) is the memory-safe Flash-Next fallback; expect "lower quality" and
  ~16-21% slower than Q2_0 for Gyro-M on 12-16 GB class cards (MODELS.md, different hardware). The app run on this
  PC (0.1.56, 3 Nov) decoded 2.1 tok/s under `--resident-experts` — do not use it as a speed path.

---

## 2. Exact launch configurations

All paths below are placeholders: put back the model folder (e.g. `/home/garyb/Mounts/NVME/strata` was the app's
`data_dir`; it is **gone** today — see §2.6). `@bundle` in the profiles resolves to the fork bundle directory
(`frontend/profiles.hpp`), so `@bundle/tools/hip/...` becomes `<bundle>/tools/hip/...`.

### 2.1 Qwen3.8-27B dense — fork dense engine (Saluki, Swift)

This is a source-build path (`build-hip/strata`; the installed app engine cannot run a `qwen35` dense file).
Profiles: `~/.config/strata/profiles/unified/saluki27-abliterated-mtp-vision.json`,
`swift27-iq3-s-mtp.json`, `swift27-iq3-s-mtp-vision.json`, `swift27-iq3-xxs*.json`.

Recommended engine command (131,072 context; `--vision` only on the vision profiles):

```sh
STRATA_Q35_DENSE=1 \
STRATA_Q35_NATIVE_LOWBIT=1 \
STRATA_Q35_KV_I8=1 \
STRATA_Q35_ATTN_SCORE_MIB=256 \
STRATA_Q35_ADAPTIVE_WEIGHTS=1 \
STRATA_Q35_ADAPTIVE_MTP=1 \
STRATA_Q35_ATTN_HEAD_REUSE=1 \
STRATA_Q35_PREFILL_LOAN=1 \
STRATA_Q35_ATTN_LT=1 \
STRATA_Q35_TOKEN_BUFFER_LOAN=1 \
STRATA_Q35_KV_GROW_STEP=4096 \
STRATA_Q35_RESERVE_MIB=1536 \
STRATA_Q35_PHYSICAL_RESERVE_MIB=1536 \
GPU_PINNED_MIN_XFER_SIZE=1000000 \
STRATA_HIPBLASLT_TUNING=$PWD/tools/hip/gfx1201-qwen35-long-context-hipblaslt-100202.txt \
build-hip/strata \
  --native /path/to/Underdog-Saluki-27B-1.0-IQ2-mix-MTP-abliterated.gguf \
  --max-context 131072 --prefill 2048 --mtp 2 --vision
```

- Same command for Swift with its GGUF. Swift profiles use `STRATA_Q35_PHYSICAL_RESERVE_MIB=1024` (measured; the
  vision profile also 1024). Saluki uses 1536/1536 (measured: 1,536 gave least free VRAM 1,546 MiB against the 768
  MiB floor, and the 1,024 run was cancelled once when the desktop held 1,637 MiB — MODEL_RESULTS "Saluki's physical
  reserve").
- `--mtp 2` is the measured draft depth for these profiles; `--prefill 2048` the measured chunk; `--vision` loads the
  CPU reader (`"exe": "@bundle/vision-cpu"`, 4 threads, up to 2,048 image tokens — costs no VRAM below 24 GB).
- `STRATA_Q35_CHECKPOINTS=8` for agent loops: measured on this PC, 118K-token 9-request conversation, total run
  4 min 11 s against 11 min 15 s without, byte-identical answers, +1,168-1,199 MiB RAM, buffers allocated at start
  (CONVERSATION_CHECKPOINTS.md; engine build `9d6bd0de`, HEAD 76ad36ce above upstream 0.2.6).
- Do not change the `STRATA_Q35_*` set piecemeal: the profiles are the measured configuration. If you replace the
  model file, re-check the profile's `auto_model`/`auto_size` fields (§2.5) or the router falls back to llama.cpp.

### 2.2 Qwen3.8-Flash-Next — fork router profile (measured) and app native-tune files

**A. Fork router profile (the measured config).** `flash-iq3.json` / `flash-q2_0.json` args, with the router's
tuning overrides (`~/.config/strata/native-tuning-router.json`: `spec_window: 3`, `vram_reserve_mib: 3584`):

```jsonc
"args": [
  "--pack", "/path/packs/iq3_xxs",
  "--native", "/path/Qwen3.8-Flash-Next-GSQ-RCO-IQ3_XXS-00001-of-00002.gguf",
  "--ple-gguf", "/path/Qwen3.8-Flash-Next-GSQ-RCO-IQ3_XXS-00002-of-00002.gguf",
  "--expert-profile", "@bundle/data/expert-profile.bin",
  "--spec", "3", "--spec-min-p", "0.5", "--mtp", "/path/mtp/rt",
  "--max-context", "131072", "--kv", "int8", "--kv-resident", "32768",
  "--expert-cache", "auto", "--prefill", "auto",
  "--resident-budget-gib", "11", "--vram-reserve-mib", "3584",
  "--adapt-async", "1", "--aux-cpus", "auto"
],
"env": {
  "STRATA_HIPBLASLT_TUNING": "@bundle/tools/hip/gfx1201-hipblaslt-100202.txt",
  "STRATA_RESIDENT_PIN": "1", "STRATA_STAGE_PIN": "0",
  "STRATA_RESIDENT_HEADROOM_GIB": "4", "STRATA_PIN_GUARD": "2",
  "STRATA_PIN_RESERVE_GIB": "4", "STRATA_UNBUFFERED_LOAD": "1"
}
```

(The profile file itself carries `--spec 4` and `--vram-reserve-mib 3072`; the router's tuning file overrides them to
3 and 3,584 — that combined configuration is what the Oct 26 table measured. Reproduce it with the tuning file, not
by editing the profile alone.)
Notes: `--kv-resident 32768` is the measured KV-streaming setting at 128K (K/V in 1.55 GiB of pinned RAM, 415 MiB in
VRAM); `--resident-budget-gib 11` is the measured RAM tier; the PLE table stays on the SSD. Do not add
`--ple-io ram`. `--adapt-async 1` and `--aux-cpus auto` are from the measured profiles.

Strata-launch equivalent for a Terminal install (`strata` is not on this PC's PATH today; the installed app started
engines directly):

```sh
strata launch --family qwen --model q2_0 --context 128k --kv int8 --vision off --no-tui
strata launch --family qwen --model iq3_xxs --context 128k --kv int8 --vision off --no-tui
```

The Terminal's generated args were **not measured on this PC**; the fork profile above is the measured one.

**B. Installed app native-tune files** (`~/.strata/data/native-tune/`, engine 0.1.56; see §2.6 for staleness):

| File | What it is now | What to change for best speed here |
| --- | --- | --- |
| `iq3_s.json` | `--kv int8 --resident-experts --vision --vram-reserve-mib 1536 --spec 4 --mtp <path> --expert-cache auto --prefill auto --max-context 131072` | **Remove `--resident-experts`** (wrong mode for 50 GB of experts on 31 GiB RAM) and add `--resident-budget-gib 11`, `--kv-resident 32768`, `--vram-reserve-mib 3584`. 1,536 is the app's file default; the measured router config went 3,072 → 3,584 after a 2,048 reserve left the worker under its 768 MiB floor (MODEL_RESULTS "Changes that did not work" and "Flash-Next at 8K"). Keep `--kv int8`, `--expert-cache auto`, `--prefill auto`. Expect the measured fork range only if the whole config matches; otherwise treat it as unmeasured. |
| `coder-iq1_m.json` | `--kv int8 --resident-experts --pool-workers 7 --vision --vram-reserve-mib 1536 --spec 4 --max-context 131072` | **Keep `--resident-experts`** — the measured app run (0.1.56, 3 Nov) held 18.93 GiB of experts in RAM with 0 file reads and decoded ~55 tok/s. `--pool-workers 7` equals the engine default on 8 cores (host core excluded), so it is redundant but harmless. This is the best Flash-Next path on this PC. |
| `gyro-gyro-s.json` | `--kv int8 --resident-experts --kv-resident 32768 --vision --vram-reserve-mib 1536 --spec 4` | Leave it if you need the smallest model; it is **not** a speed path (2.1 tok/s measured in the app log under `--resident-experts`). If used, keep context at 32K and accept the quality fallback. |
| (no file) | Q2_0 is absent | Create a `q2_0.json` from the Flash-Next template with the measured Q2_0 config: `--pack /packs/q2_0 --native …Q2_0-00001… --ple-gguf …Q2_0-00002… --expert-profile … --expert-cache auto --prefill auto --spec 3 --spec-min-p 0.5 --mtp <mtp dir> --max-context 131072 --kv int8 --kv-resident 32768 --resident-budget-gib 11 --vram-reserve-mib 3584 --vision off`. Q2_0 is the fastest full-expert Flash-Next pack measured on this PC (19.8 tok/s at 128K). |

`env` note: the app files carry `"STRATA_RESIDENT_PIN": "0"`; the measured fork profiles used
`STRATA_RESIDENT_PIN=1` plus `STRATA_RESIDENT_HEADROOM_GIB=4`, `STRATA_PIN_GUARD=2`, `STRATA_PIN_RESERVE_GIB=4`,
`STRATA_UNBUFFERED_LOAD=1`. Those are part of the measured configuration; transfer them only together with the matching
budget mode.

`--ple-io` / `--ple-row-cache`: leave `direct`; `--ple-row-cache` default 1,048,576 rows (~90 MiB) is the only PLE
lever that fits 31 GiB RAM. A larger cache is untested here.

- **Pool workers and chunk.** On this 8-core CPU the engine picks **7 CPU expert workers** (physical cores minus the
  host core) automatically; the app log confirms `CPU pool: 7 workers by default`, so `--pool-workers 7` in the app
  file is redundant. Leave `--prefill auto`: on gfx12 Linux it may take up to 16,384-token chunks, and the app log on
  this PC shows the planner refusing 16K for Coder because it would lend no extra cache slots (it kept 8,192) — the
  engine's own line is the one to read. The tuner never picks a fixed chunk below `auto` for Flash-Next ("smaller
  chunks made long prompts 11% to 75% slower", `strata_control/tune_knobs.py`).

**C. Vision.** Every Flash-Next profile reads pictures with the **CPU** reader on a <24 GB card (a few seconds per
picture, no VRAM; MODELS.md), so keeping `--vision` costs no decode VRAM. The card's reader takes ~1.3-1.4 GB of VRAM
and is not the choice here.

**D. Drafter choice.** The measured fork tables used the **MTP** drafter (`--mtp <dir>`, `--spec 3-4`). The app
engine 0.1.56 can also use the 0.2.3+ **DFlash2** drafter (`--draft dflash2 --spec 6 --draft-conf 0.7`);
the 3 Nov app runs on this PC used DFlash2 and Coder decoded ~55 tok/s. No A/B of MTP against DFlash2 exists on this
PC, and upstream's rule is `auto` = MTP ("no card has shown DFlash2 winning everywhere", release-notes-0.2.3.md;
DGX Spark measured DFlash2 slower, 37 vs 45 tok/s, DGX_SPARK_LINUX.md). Keep MTP for reproducibility; try DFlash2 as a
measure-it-yourself option. The app's own config for the model is what sets it (see §2.6 for the stale run configs).

### 2.3 Qwen3.6-35B-A3B — llama.cpp Vulkan (deployed, measured)

Profile (`tiel-coder.json` / `cyber-tiel-coder.json`): args `--native <gguf> --max-context 262144 --prefill 2048`,
`inference`: `{engine: auto (→llama for qwen35moe), device: Vulkan0, ubatch: 1024, fit: false, fit_target_mib: 2048,
cpu_moe: 26, kv: q8_0, mtp: 0}`, sampling temperature 0.6 / top-p 0.95 / top-k 20.

The router (frontend/backend.hpp) turns that into:

```sh
llama-vulkan serve --model <Tiel-Coder…UD-Q4_K_XL.gguf> --alias tiel-coder \
  --host 127.0.0.1 --port <p> --offline --no-ui --slots --metrics --parallel 1 \
  --threads 8 --threads-batch 8 --ctx-size 262144 --device Vulkan0 --flash-attn on \
  --cache-type-k q8_0 --cache-type-v q8_0 --batch-size 2048 --ubatch-size 1024 \
  --fit off --fit-target 2048 --cache-ram 0 --spec-type none --spec-draft-n-max 1 \
  --n-gpu-layers all --n-cpu-moe 26 --load-mode none --temp 0.6 --top-p 0.95 --top-k 20 \
  --mmproj <shared-vision/BF16/mmproj-BF16.gguf> --no-mmproj-offload
```

(`--no-mmproj` instead of the last two flags if pictures are not needed. The vision reader is the CPU one on this
16 GB card.)

Why 26: the router only starts a manual placement when free VRAM covers weights − CPU-layer experts + q8_0 KV +
2 GiB (backend.hpp; MODEL_RESULTS): 14,302 MiB for 26 layers at 262K. A 25-layer start was refused on this PC
("Manual placement exceeds conservative VRAM budget", first pass with the desktop at 1,591-1,955 MiB). MTP is
disabled in the llama.cpp path for MoE ("llama.cpp MoE MTP is disabled after a Vulkan device-loss trial",
backend.hpp/commit).

### 2.4 Qwen3.6-35B-A3B — Strata qwen36 engine (measured short-context win)

Source build `build-hip/strata` (same binary as the dense engine; `run()` switches on the GGUF architecture).
Measured command (commit `650737c9` block C, this PC, Oct 28):

```sh
STRATA_Q36_SKIP_MTP=1 \
STRATA_Q36_LEAN_ARENA=1 \
STRATA_Q36_EXPERT_RESERVE_MIB=1536 \
build-hip/strata --serve \
  --native /path/Tiel-Coder-35B-A3B-MTP-UD-Q4_K_XL.gguf \
  --max-context 16384 --prefill auto
```

- `--max-context` is the budget that matters: every token costs 45 KiB of fp32 K/V whether used or not. Measured
  configurations: 16,384 (8K prompt, 47.69 tok/s decode) and 40,960 (32K prompt, 26.60). Above ~40K the KV no longer
  leaves room for expert slots on 16 GB (11.0 GiB at 262K — arithmetic).
- Check the log line at load. If the file loads **without** `STRATA_Q36_SKIP_MTP=1` on the merged 0.2.10 engine, the
  Q3_K MTP block became supported (the merged engine is not the Oct 28 build); then drop the skip, add `--mtp 2` and
  draft tokens come back. That is the single thing upstream-scout is checking; until it says otherwise, skip.
- `STRATA_Q36_EXPERT_RESERVE_MIB=1536` (range 256-16384) leaves room for the desktop above the 768 MiB default
  reserve; measured least free VRAM 1,442 MiB (16K) and 1,292 MiB (40K).
- `STRATA_Q36_LEAN_ARENA=1` keeps only the experts without a VRAM slot in host RAM (measured 11.67/12.74 GiB pinned
  instead of 18.3 GiB for the default arena). It moves host copies, not kernels; token output is not claimed to
  change, but the kernel/placement tables are the same and the slot tables do not depend on it.
- Do not pass `--kv` (not an option on this path), `--resident-cpu-experts` (dense/Flash engine only), or `--ple-gguf`
  (this path has no PLE; serve mode ignores unknown flags with a log line).
- Trying it through the fork router: set the profile's `inference.engine` to `"strata"` and use the args/env above,
  keeping `engine_policy: auto`. The router's worker memory check counts a worker cgroup's clean file cache as free
  (`frontend/memory_policy.hpp`), without which a Tiel start is refused — this branch has that change.
- `--parallel` / `--batch`: each slot multiplies the KV (2 slots at 16K = 1.4 GiB); on 16 GB keep 1, as every
  measured row did. For llama.cpp keep `--parallel 1` (profile does).

### 2.5 Router profile / tuning files (`~/.config/strata/`)

Current registry: `router.json` (default `saluki27-abliterated-mtp-vision`, 9 profiles), tuning file
`native-tuning-router.json`:

```json
{"version":1,"profiles":{
 "saluki27-abliterated-mtp-vision":{"engine_policy":"auto","compute_backend":"vulkan","vision_enabled":true,"llama_fit_target_mib":1024,"memory_limit_gib":24},
 "cyber-tiel-coder":{"engine_policy":"auto","compute_backend":"vulkan","context":262144,"llama_fit_target_mib":1024},
 "flash-iq3":{"engine_policy":"auto","compute_backend":"rocm","spec_window":3,"vram_reserve_mib":3584},
 "flash-q2_0":{"engine_policy":"auto","compute_backend":"rocm","spec_window":3,"vram_reserve_mib":3584},
 "swift27-iq3-s-mtp-vision":{"engine_policy":"auto","compute_backend":"rocm"}}}
```

What to change / keep:

- **Keep** the Flash-Next overrides (`spec_window: 3`, `vram_reserve_mib: 3584`): MODEL_RESULTS says the tuning file
  sets `--spec 3` and 3,584 MiB over the profile files' 4 and 3,072, and the Oct 26 table was measured with them.
- **Dense profiles** (`saluki27-abliterated-mtp-vision`, `swift27-iq3-*`): the profile selects the fork dense engine
  through `inference.auto_engine: "strata"` + `auto_model` + `auto_size` (frontend/backend.hpp: a "measured native
  preference" only when those match the file and its size). If you move/replace the GGUF, update `auto_model` /
  `auto_size` or the route silently falls back to llama.cpp. `compute_backend: vulkan` in the tuning file applies to
  that fallback only.
- **Tiel / Cyber Tiel**: they are `qwen35moe`, so `engine: auto` resolves to llama.cpp (backend.hpp: "Dense or
  unverified MoE architecture: llama.cpp"). Keep that for 32K-262K. To try the native qwen36 path (§2.4), set
  `"engine": "strata"` in the profile's `inference` block and add the env above; flip back if it slows long prompts.
- **Parallelism**: keep `--parallel 1` / one slot. On 16 GB each slot's KV comes out of the room the experts would
  use (QWEN36.md "What a slot costs"; setup offers 2 slots at 32K on a 16 GB hybrid card, a rule measured on the
  12-16 GB cards — RTX 3060, A4000, P100 — not on this one).

### 2.6 hipBLASLt tables: only for the source build, not the app

This is a large prompt lever, and the version split is decisive:

- **Source fork build** (`build-hip/strata`, system ROCm 7.2.4) → hipBLASLt **1.2.2 = 100202**. Set
  `STRATA_HIPBLASLT_TUNING=<abs path>` to one of `tools/hip/gfx1201-hipblaslt-100202.txt` (generic 24-row),
  `gfx1201-qwen35-dense-hipblaslt-100202.txt` (9 data rows, dense 27B shapes), or
  `gfx1201-qwen35-long-context-hipblaslt-100202.txt` (67 data rows: 9 projection and 58 attention shapes; the
  profiles' choice for the dense models). The engine
  loads it only if the file's header arch `gfx1201` and its hipBLASLt version match, and prints
  `hipBLASLt tuning enabled (N rows, gfx1201, version 100202)` (`src/prefill/gemm.cu`).
  Measured on an **R9700 (gfx1201, 32 GB), hipBLASLt 1.2.2** (different card; AMD_HIP.md tuning table):
  `gfx1201-hipblaslt-100202.txt` took a 32K prompt from 638 to 1,177 tok/s and a 7K prompt from 656 to 1,164 —
  roughly 2x. That number is **not** measured on this 16 GB card.
  The tables change rounding: the 1.2.0 table's A/B on an R9700 gave a first-token KL of 0.0157 mean against plain
  over 24 prompts, top-1 the same in 24 of 24 (AMD_HIP.md). It is an opt-in, not a default.
- **Installed app engine 0.1.56** bundles hipBLASLt **1.3.0 = 100300** (`BUILD.json: "hipblaslt_version": 100300`) and
  its `tuning/` folder is absent (only `hipblaslt/library`). The 100202 tables are refused. Do **not** point the app
  engine at them. If you want a table for the app engine, calibrate one against the bundled 1.3.0 library on this card
  (`tune_hipblaslt --tuning-out`, AMD_HIP.md) and name it `gfx1201-hipblaslt-100300.txt` — that calibration has not
  been done here.
- `STRATA_HIPBLASLT_TABLE` is the packaged-engine switch (`off`/`0`/`false` disables the table beside the engine); it
  is not a way to force a mismatched table.

### 2.7 What is stale on this PC, and fresh launch commands

- `~/.strata/data/native-tune/*.json` point at `/tmp/.mount_strataFKIomL/...` (a deleted AppImage mount) for the
  `--expert-profile`, and at `/home/garyb/Mounts/NVME/strata/{models,packs}` for every model/pack/MTP path. That
  folder does not exist any more. `~/.strata/data/state.json` says setup `"complete": false`, step 3, an IQ3_S
  install cancelled at the expert-packing step; `~/.local/share/strata/install/` keeps a `.mount_strata…` path in
  every `strata-*.json` and `run-*.sh`. Any launch of any of these configs today fails on files, not on config.
- The fork router likewise resolves `@bundle/...` inside the bundle it was built in; rebuilding or replacing the
  bundle changes those paths.
- The installed app engine is fine (`strata 0.1.56`) but **no model is installed**: the app cannot start a model
  until the GGUFs are back on the data drive and it re-runs its setup/packing.

Fresh, minimal recourses (whichever install you use):

```sh
# Terminal (re)install then a model it supports (the measured fork profiles are separate):
strata launch --family coder --model IQ1_M --context 128k --no-tui
strata launch --family qwen  --model q2_0  --context 128k --no-tui
strata launch --family qwen36 --model ud-q4_k_xl --vision off --parallel 1 --no-tui

# The measured benchmark path, from this checkout (fork AppImage):
python tools/profile_context_runs.py --app /path/to/Strata.AppImage --out perf-<date> saluki27-abliterated-mtp-vision
```

Do not run more than one of these at a time: this PC has a single GPU and the measured windows required other
sessions to pause builds and browsers (MODEL_RESULTS "How the profile tables were measured").

---

## 3. Expected speed ranges (quoted; hardware and build in parentheses)

### 3.1 This PC, fork router AppImage, RX 9070 XT 16 GB (Oct 25-26 tables)

Fork build: `codex/rx9070xt-perf` `d2b0565b` + Gary's frontend commits, AppImage sha256 `0ae6890f…`, running on
upstream 0.2.6-equivalent engine; PC idle (other sessions asked to pause). "Prompt" filled to the profile's limit
unless it says 8K; decode is the fresh answer.

| Profile | Prompt tokens | Prompt tok/s (s) | Decode tok/s | Follow-up decode | Peak VRAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| Saluki IQ2-mix + MTP + vision | 8,136 | 1,317 (6) | **70.81** | 60.53 | 12,163 |
| Saluki IQ2-mix + MTP + vision | 128,974 | 762 (169) | **45.65** | 35.06 | 14,758 |
| Swift IQ3_S + MTP | 8,136 | 1,256 (6) | **65.52** | 57.03 | 15,211 |
| Swift IQ3_S + MTP | 128,974 | 740 (174) | **12.71** | 11.14 | 15,330 |
| Swift IQ3_XXS + MTP | 8,136 | 1,314 (6) | **66.84** | 55.35 | 12,788 |
| Swift IQ3_XXS + MTP | 128,974 | 751 (172) | **17.65** | 15.46 | 15,278 |
| Tiel-Coder, llama.cpp Vulkan, 26 CPU layers | 8,189 | 1,942 (4) | **41.33** | 41.21 | 13,958 |
| Tiel-Coder | 260,095 | 990 (263) | **29.78** | 30.70 | 14,289 |
| Cyber Tiel-Coder | 8,190 | 1,928 (4) | **40.43** | 43.22 | 13,796 |
| Cyber Tiel-Coder | 260,096 | 991 (262) | **30.34** | 29.88 | 14,231 |
| Flash-Next IQ3_XXS (`flash-iq3`) | 8,136 | 363 (22) | **11.63** | 12.42 | 14,128 |
| Flash-Next IQ3_XXS | 128,974 | 435 (296) | **8.98** | 16.25 | 14,808 |
| Flash-Next Q2_0 | 8,136 | 452 (18) | **16.50** | 32.24 | 14,208 |
| Flash-Next Q2_0 | 128,974 | 672 (192) | **19.76** | 31.64 | 14,558 |

The Oct 25 greedy table gives the same shapes: Saluki 128K 773 prompt / 49.25 decode; Swift IQ3_S 737 / 12.15;
Swift IQ3_XXS 755 / 17.29 and 731 / 17.94; Tiel 1,003 / 32.11; Cyber Tiel 1,001 / 31.88; IQ3_XXS 447 / 8.50;
Q2_0 675 / 18.46. At 8K the Oct 25 decodes are 2.4-4.5% higher (e.g. Saluki 72.53). Use the ranges, not one row.

### 3.2 This PC, Tiel through Strata's qwen36 engine (Oct 28, fork build `9699f5b4`)

Untimed, other sessions' work running (so conservative), `STRATA_Q36_SKIP_MTP=1`, `STRATA_Q36_LEAN_ARENA=1`,
`STRATA_Q36_EXPERT_RESERVE_MIB=1536`, no MTP drafts; sampling = the profile's, seed 20261026.

| Prompt | Context | Prompt tok/s | Decode tok/s | Experts/layer in VRAM | Pinned host | Least free VRAM | Least RAM |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 8,190 | 16,384 | 1,725 | **47.69** | 93 of 256 | 11.67 GiB | 1,442 MiB | 8.0 GiB |
| 32,742 | 40,960 | 1,135 | **26.60** | 78 of 256 | 12.74 GiB | 1,292 MiB | 8.8 GiB |

Same day/same block, llama.cpp as deployed: 32K decode **38.22 and 38.59** vs Strata 26.60. At 8K, llama.cpp 41.33
(Oct 26, sampled, another day) vs Strata 47.69. So the crossover on this PC is between 8K and 32K prompts.

### 3.3 This PC, installed app engine 0.1.56, real requests 3 Nov (logs, **not** a benchmark)

Uncontrolled load — other agent sessions ran on this PC; these are agent-chat request lines, not the bench protocol.
Useful as a reality check, not as a target.

| Config (app native-tune, engine 0.1.56) | Requests | Decode tok/s | Fresh prompt read |
| --- | ---: | --- | --- |
| Coder IQ1_M, 131K, int8 KV, `--resident-experts`, 18.93 GiB resident RAM, 0 file reads | 59 | median **54.7** (p25 52.0, p75 58.4, max 67.6) | median 531 tok/s (40-84K-token conversations) |
| IQ3_S, 131K, int8 KV, `--resident-experts` | 2 | 0.8 and 8.3 | 68 tok/s at 7.3K |
| Gyro-S, 131K, int8 KV, `--resident-experts --kv-resident 32768` | 2 | 2.1 | 49 tok/s at 7.2K |

### 3.4 Numbers from other hardware (do not read as this card's speed)

- **R9700 32 GB, gfx1201, 0.1.45** (QWEN36.md): Qwen3.6 UD-Q4_K_XL decode **117.8** tok/s with every expert in VRAM,
  prompt pp4096 6,220 tok/s at chunk 4096; `--mtp 2` 183.7 tok/s (10-prompt median); `--batch 8 --mtp 0`: 1/2/4/8
  requests 122.4/205.1/303.3/386.9 tok/s. UD-Q6_K_XL decode 106.9.
- **R9700 32 GB, 0.2.7, 125 GB RAM** (release-notes-0.2.7.md): IQ3_S 8K prompt 2,505 tok/s, decode 94.6; 32K prompt
  2,802, decode 85.1.
- **R9700 32 GB, hipBLASLt 1.2.2** (AMD_HIP.md): gfx1201-hipblaslt-100202 table 32K prompt 638→1,177 tok/s.
- 32 GB card class, 8-bit KV, MTP (QUANTS.md, one R9700): IQ3_S 75-95 tok/s; UD-Q4_K_XL 63-88; UD-Q6_K_XL 54-76.
- **RTX 3060 12 GB hybrid** (QWEN36.md): Qwen3.6 decode 59.7 tok/s; Flash-Next (MODELS.md history, RTX 5070 12 GB,
  engine 0.1.26): Q2_0 94 tok/s short/76 at 128K, Coder 55/43.
- **RX 9070 XT 16 GB, engine 0.1.30, Ryzen 9 3900X, 47 GB RAM, ROCm 7.14 KVM guest** (AMD_HIP.md): Coder IQ1_M
  32K context — 4K prompt 782 tok/s, decode 30.8; 16K prompt 1,235, decode 35.4.
- **RX 9070 XT 16 GB, Windows, engine 0.1.34** (AMD_HIP.md, #325): Coder IQ1_M 32K decode 29.2 tok/s, prefill ~181.
- **RX 9070 XT 16 GB, 47 GB RAM, engine before 0.1.40** (MODELS.md history): Q2_0 60 short/48 at 128K, 32K prompt
  1,160; IQ2_XS 52/36; Coder 44/33, 32K prompt 1,420.

### 3.5 Expected effect of the upstream 0.2.7-0.2.10 gfx12 defaults on this PC (unverified here)

The Oct 25-26 tables **predate** these, so the merged engine should be a little faster; exact effect on a 16 GB card
is unmeasured. Defaults now on for gfx12 (all measured on R9700, a different card):

- PCIe miss split starts at `--pcie-frac 0.15` (gfx12): IQ3_S decode 86.1→94.6 tok/s at 8K, 67.2→84.4 at 32K
  (AMD_HIP.md). The 16 GB card has a higher miss rate, so the direction should be the same but the size is unknown.
- Matrix-core prompt attention on (gfx12 Linux, needs int8 KV); fused int8 prompt experts on for the IQ3_S/IQ3_XXS
  packs on gfx12 Linux; the matrix-core QSA block scorer past ~28K tokens (+5.7% prompt at 98K on an R9700, first-token
  KL 0.002 above 30K, so near-exact rather than bit-exact); HIP queue priming on gfx1201. The app 0.1.56 log on this PC
  shows the expected startup lines (`gfx12 (RDNA4):` switch lines, `PCIe probe ... -> pcie_frac 0.15`, `the prompt
  attention on matrix cores on`).
- Bare `--prefill auto` may now take 16,384-token chunks on gfx12 Linux (the app log on this PC showed it chose 8,192
  because 16K would lend no more cache slots — the engine's own planner decides, and that line is the one to read).
- HIP builds of 0.2.10 default to **16-row verify windows** (`CMakeLists.txt`, `STRATA_WINDOW_MAX_T=auto` → 16 for
  HIP), so `--spec` values above 4 may pay for Flash-Next/Qwen3.6 on this engine where they did not before. The
  measured fork tables were built on the 8-row window. This is a trial knob, not a recommendation: the tuner's safe
  bounds are `--spec` 3-4 (`strata_control/tune_knobs.py`), and `--spec` changes which tokens are accepted.
- 0.2.7 changed answers on near-ties through several summation-order changes (release notes; DETAILS.md
  `STRATA_DEFAULTS=026`), so do not compare a merged-engine answer token-for-token with the Oct tables.

---

## 4. Fastest path, ordered per model

**Qwen3.8-27B dense**
1. Saluki IQ2-mix + MTP, fork dense engine, 131,072 ctx, reserves 1,536/1,536, `--mtp 2 --prefill 2048`,
   long-context hipBLASLt 100202 table: 70.8 tok/s at 8K, 45.7 at 128K. Fastest measured model on this PC.
2. Swift IQ3_XXS + MTP, same config (physical reserve 1,024): 66.8-69.1 at 8K, 15.9-17.9 at 128K. Take this when
   IQ2-mix quality (and the abliterated tune) is not acceptable at long context.
3. Swift IQ3_S + MTP only at ≤32K (measured 8K: 65.5-68.6). At 128K it decodes ~12 tok/s (weight spill) — avoid
   unless fidelity is the only criterion.
4. Add `STRATA_Q35_CHECKPOINTS=8` for agent workflows (4 min 11 s vs 11 min 15 s measured at ~118K).

**Qwen3.8-Flash-Next**
1. Coder IQ1_M, app engine 0.1.56, 131K, int8 KV, `--resident-experts` (23 GB experts fit RAM): ~55 tok/s measured
   in app logs; code-only quality.
2. Q2_0, fork Flash engine, 131K, int8 KV + `--kv-resident 32768` + `--resident-budget-gib 11` + 3,584 MiB reserve:
   16.5-19.8 tok/s decode, 452-672 prompt (Oct 26). The fastest full-expert pack here; quality "good" (80.6% top-token
   of Q8_0).
3. IQ3_XXS same settings: 9.0-11.6 tok/s; better quality (84.5%).
4. IQ3_S: no — 50 GB of experts cannot be held on 31 GiB RAM; measured app run 0.8-8.3 tok/s. It is the best quality
   (87.7%) but not a speed path on this PC.
5. Gyro-S only as a memory fallback (lower quality, slower).

**Qwen3.6-35B-A3B**
1. Prompt ≤ ~16K tokens: Strata qwen36 engine, `--max-context 16384`, skip MTP + lean arena + reserve 1,536:
   47.69 tok/s at 8K, 1,725 prompt. Best measured Tiel decode on this PC.
2. Prompt 32K-262K: llama.cpp Vulkan, 26 CPU expert layers, q8_0 KV: 38.2-41.3 tok/s at 32K/8K, 29.8-32.1 at 262K.
3. Do not use native qwen36 above ~40K context (fp32 KV 1.4 GiB at 32K, 5.5 GiB at 128K, 11 GiB at 262K; no room
   left for experts on 16 GB).
4. Watch the Q3_K MTP gate: if the merged 0.2.10 engine loads the MTP block, native decode gets the draft bonus and
   the crossover moves outward; if not, skip it as above.

**Other**
- No other documented LLM fits 32 GB RAM + 16 GB VRAM at a usable speed: Qwen3.6 Q6/Q8 need 44/52 GB RAM;
  Flash-Next Unsloth needs 72-88 GB; DeepSeek needs 46 GB+ and 239-274 GB disk. A higher-quant dense 27B is
  undocumented/unmeasured here.

---

## 5. Not tested / risks

- **No model files are installed on this PC** (`/home/garyb/Mounts/NVME/strata` is gone; app setup incomplete,
  cancelled at packing). Nothing in this document was run; all numbers are quoted or arithmetic.
- **The merge is not measurable yet.** The dense 27B tables (Oct 25-26) are from fork `d2b0565b` on the 0.2.6
  rebase; the branch now merges upstream 0.2.10 (engine 0.1.56). No 0.2.10 fork build has been benchmarked on this
  PC — that is the build-validation teammate's job. Dense logits were bit-identical between `d2b0565b` and the 0.2.6
  rebase (MODEL_RESULTS "Not measured"), but not against 0.2.10.
- **Installed-app vs fork-engine split.** The installed 0.1.56 app engine cannot run the dense 27B (`qwen35` dense is
  fork-only: `STRATA_Q35_DENSE=1`; the upstream source at `ffa80d73` has no dense Qwen35 path — `git grep` finds none
  — and the merge commit keeps the fork's dense engine). The dense numbers need the fork build. The app engine also
  bundles hipBLASLt 100300, so the 100202 tables that drove the R9700 prompt results **do not apply** there.
- **32K is extrapolated** for the dense 27B (only 8K and 128K were measured). The IQ3_S 128K slowdown is real and
  measured; where on the 8K-128K path it starts is not known.
- **Tiel native measurements are untimed** and were taken with other sessions' work running (commit `650737c9`
  itself says so); they also predate the 0.2.10 merge. Tiel's Q3_K MTP block is the open gate: native load fails
  without `STRATA_Q36_SKIP_MTP=1` on the Oct 28 build, and whether 0.2.10 accepts Q3_K is being checked.
- **No A/B of MTP against DFlash2** on this PC; the app logs used DFlash2 while the measured tables used MTP. MTP is
  what the docs recommend (`--drafting auto` = MTP).
- **App 0.1.56 log numbers are not benchmark numbers** (real requests, uncontrolled load; the IQ3_S/Gyro-S runs were
  starved by memory). They are included only as a reality check.
- **`--spec > 4`** may now be viable on HIP's 16-row windows but is untested; the tuner's safe range is 3-4 and
  `--spec` changes accepted tokens.
- **PLE row cache** (`--ple-row-cache`) is a plausible RAM-sized lever for the Flash-Next prompt bottleneck; no
  measurement exists for it here.
- **Near-tie answer changes.** Upstream 0.2.7-0.2.10 changed summation orders and defaults (PCIe split 0.15, fused
  prompt experts, WMMA prompt attention, QSA block scorer, route tail skip) and several of those change the last
  bits; greedy answers can diverge at near-ties. Use `STRATA_DEFAULTS=026` / `STRATA_GFX12_DEFAULTS=0` only for A/B,
  not for the fast path.
- **gfx1201 vs R9700.** The R9700 numbers (117.8 tok/s Tiel all-in-VRAM; 94.6 IQ3_S; 2x prompt from the table) are a
  32 GB card with lower miss rates; do not carry them to the 16 GB 9070 XT, where the same settings leave most
  experts on the CPU or SSD.
- **Desktop VRAM drift** (1,115-2,026 MiB measured) can cancel a long request when little VRAM is left; that is what
  the 768 MiB worker floor and the 1,536/3,584 MiB reserves are for. Re-run with the browser closed when measuring.
- **Vision** costs no VRAM with the CPU reader, but it loads a separate process and a few seconds per picture; keep it
  only if pictures are used.
- Not covered: Windows/CUDA/SYCL, multi-GPU, any model file not named above, any pack not in the tree's docs.
