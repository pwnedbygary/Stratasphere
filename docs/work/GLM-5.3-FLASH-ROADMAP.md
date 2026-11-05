# GLM-5.3-Flash on this PC: what it would take

Status: **not supported by any Strata engine today** (neither this fork nor upstream 0.2.10 has a GLM model family —
`git grep -i glm` over `docs/ src/ include/ setup.py` finds no architecture code). This page records what the model
is, what exists on Hugging Face, and the concrete path to "eventually" run it here. It is a plan, not a claim.

## The model

`zai-org/GLM-5.3-Flash` (Hugging Face, 305.8 GiB of BF16 safetensors; `architectures: Glm5NextForConditionalGeneration`,
`model_type: glm5_next`). From its `config.json`:

| Property | Value |
|---|---|
| Layers | 45 |
| Attention | **hybrid**: 34 `linear_attention` layers + 11 `deepseek_sparse_attention` layers (MLA-NOPE, `mla_use_nope: true`) |
| Linear attention (KDA) | 64 heads, head_dim 128, short-conv kernel 4, gate lower bound -5, `kda_layers` list |
| Sparse attention | MLA: `kv_lora_rank 512`, `q_lora_rank 1536`, `qk_nope_head_dim 256`, `v_head_dim 256`, `qk_rope_head_dim 0` |
| Hyper-connections | `mhc: true`, `hc_mult 4`, `hc_sinkhorn_iters 20` (Manifold-Constrained Hyper-Connections) |
| Experts | 288 routed, 8 per token, `moe_intermediate_size 2048`; first 3 MLP layers dense (`mlp_layer_types`) |
| Indexer | `index_n_heads 32`, `index_head_dim 128`, `index_topk 2048`, full on all layers |
| Context | 1,048,576 positions natively |
| Vocab | 154,880 |
| Size / active | 320 B total, 18 B active |

## What exists on Hugging Face

- `unsloth/GLM-5.3-Flash-GGUF` (2,370 GiB total): UD-IQ1_S **86.7 GiB**, UD-IQ1_M 90.9, UD-IQ2_XXS 94.9,
  UD-Q2_K_XL 101.3, UD-IQ3_XXS 112.1, UD-IQ4_XS 146.1, UD-Q3_K_XL 137.4, UD-Q4_K_XL 186.0, Q8_0 317.6, BF16 597.6.
- Also `huihui-ai/…-abliterated-GGUF`, `orcarouter/…-Uncensored-GGUF`, `ukisai/Swift-1.5-GLM-5.3-Flash-GGUF`,
  `mradermacher/…-REAP50-…`, `nvidia/GLM-5.3-Flash-FP4-Local`, `zai-org/GLM-5.3-Flash` (base).
- Unsloth's card says to run it with **llama.cpp PR #27754** or Unsloth Desktop — i.e. support is in that PR, not
  (necessarily) in the old `llama` 0.4.0-dev build installed at `~/.local/bin/llama` (its strings carry `glm4`,
  `glm4moe`, `glm4v`, not `glm5_next`).

Fit on this PC (16 GB RX 9070 XT, 30 GiB usable RAM): the smallest quant (UD-IQ1_S, 86.7 GiB) cannot live in
VRAM+RAM; it would stream most experts from the NVMe SSD. That is the same regime that leaves DeepSeek slow here,
and GLM has no Engram (its cost is the KDA/mHC state plus expert streaming). It will run, slowly; no measured
number exists yet.

## Path A — interim: llama.cpp with PR #27754 (days)

1. Build llama.cpp at the PR revision (`gh pr checkout 27754` or the PR's head commit) for gfx1201.
2. Fetch one quant — UD-IQ1_S (86.7 GiB) is the only one that fits the current free space after DeepSeek; UD-IQ1_M
   (90.9) if ~4 GiB more can be freed.
3. Run `llama-cli`/`llama-server` with `-ngl`/`--n-cpu-moe` tuned for 16 GB VRAM + 30 GiB RAM; expect single-digit
   tokens/s and a long first load (the file is mmap'd, experts stream).
4. This gives a working reference for correctness (logits/answers) that a Strata port can be validated against.

## Path B — a Strata `glm5_next` family (the "eventually"; weeks not days)

Strata's engines are explicit per-architecture programs. GLM-5.3 combines pieces that exist separately in two of
them, plus one that exists in neither:

| Piece | Exists in | Reuse |
|---|---|---|
| MoE routing + expert kernels (288 experts, top-8) | `qwen4exp` (Qwen3.8-Flash-Next) and `dsv41` (DeepSeek) | high |
| Hyper-connections (mHC, sinkhorn) | `qwen4exp` (HC projections); sinkhorn iterations are new | medium |
| Sparse MLA-NOPE attention | `dsv41` (MLA / Engram engine) | medium-high |
| Indexer / QSA selection | `qwen4exp` (indexer, top-k selection) | medium |
| **KDA linear attention** (64 heads, short conv, gates) | none | **new kernel family** |
| Hybrid KV: recurrent KDA state + MLA cache | neither | **new cache/session layout** |
| Tokenizer/template (glm5 vocab 154,880) | tokenizer tooling is generic (`tools/strata_tokenizer.py`) | high |

Concrete work items, in order:

1. **Intake**: a `glm5` family in `include/strata/core/model_family.hpp` + `geometry.hpp` translating the GGUF's
   `glm5_next.*` keys; unsloth's GGUF metadata is the reference (`llama.cpp` PR #27754 shows the mapping).
2. **Pack**: extend `tools/dsv41/pack_gguf.py` (or a new `tools/glm5/…`) to emit the tensor map, tokenizer and chat
   template for this geometry.
3. **KDA kernels**: gated linear attention with short conv (kernel 4), per-head gate, chunked parallel form for
   prefill and a decode step; parity against the llama.cpp PR on the same prompts.
4. **mHC / sinkhorn**: HC projections, the 4-way mixer and 20 sinkhorn iterations on the hyper-connection matrices.
5. **Sparse MLA layers** and the indexer path (11 layers): reuse the `dsv41` MLA math and `qwen4exp` indexer top-k.
6. **Hybrid state**: one session cache holding the 34 KDA recurrent states + the 11 MLA layers' KV; context up to 1M
   (occupies VRAM first; plan the RAM/SSD tiers as `dsv41` does).
7. **Validation gates** (the project's rules): logits parity against the llama.cpp PR over a fixed prompt set;
   the standard 10-prompt bit/answer gate; a 16-token smoke answer; needle tests at 8K/32K; a measured speed report
   on this card and RAM size. Only then does a default change.

Effort estimate (rough, static): intake + tokenizer/pack 3-5 days; KDA + mHC kernels and the hybrid cache 2-4 weeks;
parity/quality/performance work 1-2 weeks; total **4-7 weeks** of focused engine work with an AMD card to test on.
That is why it is "eventually": nothing here should promise GLM support before those gates pass.

## Practical recommendation

- Keep DeepSeek on `strata-dsv41` (now built from this tree) for the next step; it is a supported engine family.
- For GLM-5.3-Flash today, prefer the llama.cpp PR route; it also gives the oracle a Strata port would be checked
  against.
- Do not download a GLM quant yet unless the llama.cpp path is chosen: the smallest is 86.7 GiB and the disk
  currently holds the DeepSeek Coder set.
