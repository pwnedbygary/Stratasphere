# Downloaded models (2026-11-05)

Fetched with the Hugging Face CLI (`hf download`) into this folder, plus the local artefacts Strata needs to serve
them (native packs, tokenizers and the Flash-Next MTP draft runtime). Every download was checked against its
repository's size and LFS SHA-256 (`.download-logs/verification.json`).

## Models

| Folder | Files | Size | Source repository | Router profile |
|---|---|---|---|---|
| `Saluki-27B/` | `Underdog-Saluki-27B-1.0-IQ2-mix-MTP-abliterated.gguf`, `vision/F16/mmproj-Underdog-Saluki-27B-1.0-F16.gguf` | 8.6 GiB | `Kujira/Underdog-Saluki-27B-1.0-MTP-Abliterated-GGUF`, `ConwayResearch/Underdog-Saluki-27B-1.0` (vision) | `saluki27-abliterated-mtp-vision` |
| `Swift-1.5-27B/` | `Swift-1.5-Qwen3.8-27B-GSQ-RCO-IQ3_S-mtp.gguf`, `...IQ3_XXS-mtp.gguf`, `...IQ3_XXS.gguf`, `vision/F16/mmproj-Swift-1.5-Qwen3.8-27B-F16.gguf` | 31.6 GiB | `ukisai/Swift-1.5-Qwen3.8-27B-GSQ-RCO-GGUF`, `ukisai/Swift-1.5-Qwen3.8-27B-GGUF` (vision) | `swift27-iq3-s-mtp-vision`, `swift27-iq3-xxs-vision` |
| `Qwen-Flash-Next-Unsloth/` | `UD-IQ3_XXS/…-0000{1..4}-of-00004.gguf`, `mmproj-BF16.gguf` (vision) | 78.7 GiB | `unsloth/Qwen3.8-Flash-Next-GGUF` (the Unsloth set you were using) | new `flash-unsloth-iq3_xxs` |
| `Qwen-Flash-Next-Coder/` | `IQ1_M/Qwen3.8-Flash-Next-GSQ-RCO-IQ1_M-0000{1,2}-of-00002.gguf`, `mmproj-Qwen3.8-Flash-Next-BF16.gguf` | 56.1 GiB | `ISTA-DASLab/Qwen3.8-Flash-Next-GSQ-RCO-Coder-GGUF` (revision `5348543e`) | `flash-coder-iq1_m` (app server; not in the router registry) |
| `Cyber-Tiel-Coder-35B-A3B/Q4_K_XL/` | `Cyber-Tiel-Coder-35B-A3B-MTP-UD-Q4_K_XL.gguf`, `mmproj-BF16.gguf` | 22.0 GiB | `peculiar-ragdoll/Cyber-Tiel-Coder-35B-A3B-GGUF-MTP` | `cyber-tiel-coder` |

The ISTA-DAS Flash-Next set (Q2_0 + IQ3_XXS, 133.4 GiB) was **removed on 2026-11-05** by choice: the newer Unsloth
Dynamic v3.1 UD-IQ3_XXS replaced it as the only Qwen3.8-Flash-Next pack here, freeing 134 GiB.

**DeepSeek is archived (2026-11-05).** Its GGUF files (Coder shards, Engram, drafter, vision encoder, metadata shard)
now live on the RAID array at `/home/garyb/Mounts/RAID/Models/DeepSeek-V4.1-Flash-Coder-IQ2_XXS/` (179 GiB), which
freed 179 GiB on the NVMe. The engine pack (6.8 MB), the two configs (`strata-deepseek-coder.json`, the 1M one) and
`run-dsv41.sh` stay here, so the profile survives; `ARCHIVE-NOTES.md` in that folder has the SHA-256 list and the exact
restore commands. It was archived because this 30 GiB-RAM PC runs it at 2.1 tok/s and a 1M-token prompt needs ~51 min
to the first token — it will go back on the NVMe when RAM is cheaper.

## Locally generated artefacts (not downloads)

| Path | What it is | Built with |
|---|---|---|
| `packs/coder-iq1_m/` | native pack for the Coder IQ1_M shards (index 1079 tensors, 302 served natively, arena 1.37 GiB, tokenizer/) | `tools/iq_pack.py` |
| `packs/ud-iq3_xxs/` | native pack for the Unsloth UD-IQ3_XXS file (Flash-Next, vision) (index.txt 1079 tensors, dense.bin 1.38 GiB, 195 tensors converted to BF16, tokenizer/) | `tools/iq_pack.py --compat-bf16` |
| `tokenizers/saluki27-abliterated/` | tokenizer for the Saluki profile | `tools/strata_tokenizer.py` |
| `tokenizers/swift27-iq3-s-mtp/`, `tokenizers/swift27-iq3-xxs/` | tokenizers for the Swift profiles | `tools/strata_tokenizer.py` |
| `mtp/src/` | the 5.2 GB of BF16 `mtp.*` tensors from `Qwen/Qwen3.8-Flash-Next`, hash-verified | `tools/mtp_fetch.py fetch/verify` |
| `mtp/rt/` | the Flash-Next MTP draft runtime (`experts.bin`, `dense.bin`, `dense.txt`) the engine's `--mtp` expects | `tools/mtp_pack.py --experts q2_0`, `tools/mtp_rt.py` |
| `shared-vision/BF16/mmproj-BF16.gguf` | shared vision encoder for the Cyber-Tiel profile | copy of the Cyber-Tiel `mmproj-BF16.gguf` |
| `.venv-tools/` | Python venv (system site packages) with `regex`, used by the pack/tokenizer/MTP tools | `python3 -m venv --system-site-packages` |

## Vision-only profile set (2026-11-05)

Every router profile kept here now has an image reader; the two text-only Swift profiles (`swift27-iq3-s-mtp`,
`swift27-iq3-xxs`) were removed by request. The Unsloth Flash-Next profile gained `--vision` plus the Unsloth
`mmproj-BF16.gguf` encoder, and the DeepSeek run script/config now start with `--vision on --reserve-mib 900` (its pack
already carries the picture tower).

## Router profiles repointed

`~/.config/strata/profiles/unified/*.json` were rewritten (backup in
`~/.config/strata/backups/pre-LLMs-repoint-*`): every old `/home/garyb/Mounts/NVME/Strata/...` path and every
`@bundle/...` reference now points at the files above, the source-built engine
(`/home/garyb/Mounts/NVME/.agent-work/build-hip-0210-fixed/strata`), the repo's `data/expert-profile.bin`,
`tools/hip/*.txt` tuning tables and the installed `strata-vision` helper. `router.json` gained the
`flash-unsloth-iq3_xxs` profile.

## Smoke tests

- Dense: the engine loaded the Saluki GGUF with the profile's env, prefill ran, hipBLASLt long-context table was
  accepted (67 rows, gfx1201, 100202), 1 token generated (log: `.download-logs/smoke-saluki.log`).
- Unsloth UD-IQ3_XXS: hash-verified against the HF LFS metadata (`.download-logs/verify-unsloth.out`), pack built,
  smoke test in `.download-logs/smoke-flash-unsloth.log`; vision encoder `mmproj-BF16.gguf` added, profile carries `--vision`.

## Vision verification (2026-11-05)

Every model set that supports pictures was exercised, not just path-checked. For the four llama.cpp-`mtmd` encoders the
installed `strata-vision` was started with each model's own `--mmproj/--model` pair and asked for `ENC shapes.png`
(`tools/qwen35/pictures/shapes.png`, 640x400); DeepSeek's engine tower was exercised through
`strata-dsv41 --encode-image` (its tower requires sides that are multiples of 14, so 644x406).

| Model | Encoder | Embedding width | Image tokens | Output |
|---|---|---|---:|---:|
| Saluki 27B | `mmproj-Underdog-Saluki-27B-1.0-F16.gguf` | 5120 | 260 (20x13) | 5.32 MB |
| Swift 1.5 27B (IQ3_S / IQ3_XXS) | `mmproj-Swift-1.5-Qwen3.8-27B-F16.gguf` | 5120 | 260 (20x13) | 5.32 MB |
| Flash-Next Unsloth UD-IQ3_XXS | `mmproj-BF16.gguf` (Unsloth) | 2560 | 260 (20x13) | 2.66 MB |
| Flash-Next Coder IQ1_M | `mmproj-Qwen3.8-Flash-Next-BF16.gguf` (Coder repo) | 2560 | 260 (20x13) | 2.66 MB |
| Cyber-Tiel Coder 35B-A3B | `mmproj-BF16.gguf` | 2048 | 260 (20x13) | 2.13 MB |
| DeepSeek V4.1 Coder | pack tower (`vision` 0.97 GB) | — | 172 rows at 644x406 | 1.76 MB |

All five profiles carry a `vision` block pointing at these files, and the three engine profiles also pass `--vision`;
DeepSeek's run script/config use `--vision on --reserve-mib 900`. The shared `shared-vision/BF16/mmproj-BF16.gguf` is
byte-identical (md5 `a6d3c7a0…`) to the Cyber-Tiel encoder it was copied from.

## Final setup (2026-11-05, after the benchmark gauntlet)

Full numbers: `/home/garyb/Mounts/NVME/LLMs/bench-results/gauntlet-20261105/RESULTS.md`.

| Model | Profile | Max context set | Why |
|---|---|---|---|
| Saluki 27B IQ2-mix + MTP | `saluki27-abliterated-mtp-vision` (**router default**) | 131,072 | Best all-round: 72.3/62.4/49.2 tok/s at 8K/32K/128K, 13/13 code checks at every window |
| Flash-Next Coder IQ1_M | `flash-coder-iq1_m` (app server, `strata-coder`) | 131,072 | Fastest 8K/128K: 73.1 tok/s at 8K, 2,170 tok/s prefill and 54.8 tok/s decode at 128K; 12/13 |
| Cyber-Tiel Coder 35B-A3B | `cyber-tiel-coder` | 262,144 | The only >128K option, now with **MTP drafts** (opt-in patch, ubatch 512): 8K 59.5 tok/s decode, 260K prompt 748.9 prefill / 38.3 decode; 12/13 |
| Swift 1.5 IQ3_XXS / IQ3_S | `swift27-iq3-*-vision` | **32,768** | 128K collapses (20.4 / 13.0 tok/s); at 32K they are 67.9 / 67.3 with 13/13 |
| Flash-Next Unsloth UD-IQ3_XXS | `flash-unsloth-iq3_xxs` | **32,768** | Quality-only (~5 tok/s decode); 128K is 4.9 tok/s |
| DeepSeek V4.1 Coder | `DeepSeek-Coder/` configs | 32K practical (1M available) | Archived; 1M config loads but ~51 min to first token here |

- The router default stays **Saluki**. The Coder is deliberately **not** in the router registry: the native server's
  memory admission refuses its 17.5 GiB resident expert tier at long prompts; the app-server path
  (`~/.local/bin/strata-coder`, port 8097) is the working one. Cyber-Tiel and the dense/Swift/Flash profiles are in
  the router; `strata-router` launches it on 0.0.0.0:8096 with the API key.

## Storage benchmark: NVMe vs the SATA RAID0 array (2026-11-05)

Both with direct I/O; RAID0 = the two SATA SSDs, btrfs single 3.7 TB array. Sequential 2 GiB, fio 4K random read.

| Test | NVMe (nvme0n1) | RAID0 SATA | Winner |
|---|---:|---:|---|
| Sequential read | 1.6 GB/s | 1.0 GB/s | NVMe |
| Sequential write | 753 MB/s | 406 MB/s | NVMe |
| 4K random QD1 | 5,447 IOPS / 0.183 ms | 8,169 IOPS / 0.122 ms | RAID0 |
| 4K random QD32 | 67.9–70.6k IOPS / 265–276 MiB/s | 148–151k IOPS / 578–591 MiB/s | RAID0 |
| The 179 GiB archive move | source | ~610 MB/s sustained | — |

So the RAID0 array is a good archive: it beats this NVMe on random 4K latency/IOPS (the part that matters if a model
streams experts) and gives up ~1.6x sequential speed (the part that matters for first load). Moving DeepSeek there
costs nothing for cold storage.

## Disk after the archive

- NVMe (`/home/garyb/Mounts/NVME`): **348 GiB free** (was 172 GiB before archiving).
- RAID (`/home/garyb/Mounts/RAID`): 2.7 TiB free, DeepSeek archive 179 GiB.

## Cyber-Tiel MTP (local opt-in patch, 2026-11-05)

The Cyber-Tiel GGUF carries its own MTP block (blk.40, `nextn_predict_layers = 1`), but the fork's router refused
llama.cpp MTP on MoE models after a Vulkan device-loss trial (and on manual CPU-expert placement). The file now has
an opt-in: `frontend/backend.hpp` skips both guards only when the profile sets `inference.allow_mtp: true` (commit
`c3f5f5c5`); the Cyber profile sets `{"mtp": 2, "allow_mtp": true, "ubatch": 512, "cpu_moe": 26}`.

Measured through the rebuilt AppImage (full profile, vision encoder loaded):

| Window | Decode before | Decode with MTP | Draft acceptance |
|---|---:|---:|---:|
| 8K | 30.9 tok/s | **59.5 tok/s** | 188/210 (90%) |
| 260K prompt | 24.2 tok/s | **38.3 tok/s** (748.9 prefill) | 186/216 (86%) |

`ubatch 1024` is ~10% faster at 8K (65.8) but its 260K prompt trips llama.cpp's memory guard (VRAM floor 768 MiB);
`ubatch 512` is stable at 262,144 tokens. No device loss was observed. Rollback: the previous AppImage is
`~/Applications/strata_model_router.appimage.bak-20261105-cybermtp` and the previous profile is
`cyber-tiel-coder.json.bak-20261105-cybermtp`; set `inference.mtp` back to 0 / remove `allow_mtp` to disable drafts
without reverting the binary.
