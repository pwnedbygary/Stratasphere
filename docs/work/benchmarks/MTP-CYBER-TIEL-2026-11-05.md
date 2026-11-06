# Cyber-Tiel MTP A/B (llama.cpp Vulkan, direct, no router)

Model: `Cyber-Tiel-Coder-35B-A3B-MTP-UD-Q4_K_XL.gguf` (MTP block blk.40 is present: 20 tensors, Q3_K experts).
Binary: `~/.local/bin/llama serve` (Vulkan, same build the router bundles). Flags mirror the Cyber profile:
`--n-cpu-moe 26 --load-mode none --fit off --n-gpu-layers all --cache-type-k/v q8_0 --ubatch 1024`,
213-token prompt, greedy, 256 max tokens.

| Context | `--spec-type none` | `--spec-type draft-mtp` | Gain | Drafts |
|---|---:|---:|---:|---|
| 8,192 | **51.72 tok/s** | **72.25 tok/s** | **+39.7%** | 102/148 accepted (69%), mean 2.38 |
| 262,144 | **40.38 tok/s** | **49.67 tok/s** | **+23.0%** | 102/148 accepted (69%), mean 2.38 |

No device loss, no error in either run; servers exited cleanly. The rest of the prompt/decode numbers
(prompt 213 tokens: 340 vs 311 tok/s at 8K; 279 vs 255 at 262K) are within noise.

Why the router does not use this: `frontend/backend.hpp` deliberately refuses llama.cpp MTP on MoE models
("llama.cpp MoE MTP is disabled after a Vulkan device-loss trial; use MTP off. Native Strata MTP is separate."),
and the Cyber profile also uses manual CPU-expert placement (`cpu_moe 26`, `fit: false`), which its
"Manual placement with llama.cpp MTP is not memory-validated" guard rejects too. Strata's native qwen36
engine supports MTP, but Tiel-Coder's MTP block is Q3_K and its GPU expert kernels cannot take it.

Logs: `baseline.log`, `mtp.log` here; at 262K in `../mtp-test-262k/`.

## Through the router (patched, real Cyber profile)

The fork now has an opt-in `inference.allow_mtp` (commit `c3f5f5c5`); the Cyber profile sets
`{"mtp": 2, "allow_mtp": true}`. Router AppImage rebuilt; profile kept at 262,144 context with the vision encoder.

| Run (router, full profile) | Prefill tok/s | Decode tok/s | Follow-up | Drafts | Code |
|---|---:|---:|---:|---:|---:|
| 8K, MTP, ubatch 1024 | 1,888.9 | **65.83** | 52.36 | 191/204 (94%) | 12/13 |
| 8K, MTP, ubatch **512** (final) | 1,284.7 | **59.51** | 50.74 | 188/210 (90%) | 12/13 |
| 260,096 tokens, MTP, ubatch **512** (final) | 748.9 | **38.34** | 30.81 | 186/216 (86%) | 12/13 |
| Pre-MTP baselines (same profile, older build) | 1,858.9 / 970.2 | 30.94 / 24.22 | 31.05 / 25.68 | none | 12/13 |

`ubatch 1024` + MTP passes an 8K prompt but **fails a 260K prompt**: the llama.cpp memory guard stops the worker at
642 MiB free VRAM (floor 768) because the MTP block adds weights on top of the 262K KV. `ubatch 512` keeps it above
the floor and completes the full 260K prompt; it costs ~10% on short-prompt speed (65.8 -> 59.5) and is the final
setting. No device loss and no errors in any of these runs.
