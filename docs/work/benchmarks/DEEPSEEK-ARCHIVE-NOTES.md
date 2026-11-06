# DeepSeek V4.1-Flash Coder IQ2_XXS — archived 2026-11-05

Moved off the NVMe (/home/garyb/Mounts/NVME/LLMs) to free ~179 GiB. The engine pack, configs and launch script
stay on the NVMe, so the profile survives; the model files below are needed before it can start.

## What is here

| Path here | Original path | Size | SHA-256 |
|---|---|---:|---|
| `LLMs/DeepSeek-Coder/IQ2_XXS/*.gguf` | `~/Mounts/NVME/LLMs/DeepSeek-Coder/IQ2_XXS/` | 67.2 GiB (5 shards) | pinned in `/home/garyb/LLM-Projects/Strata/data/dsv41/coder-set.json` |
| `LLMs/DeepSeek-Coder/mmproj-DeepSeek-V4.1-Flash-1017-BF16.gguf` | `~/Mounts/NVME/LLMs/DeepSeek-Coder/` | 0.91 GiB | `0fdc4b7693f287e9665940cf42bd39c546ffe4f0def89c569d7d0e45d60d2df1` (pin in coder-set.json) |
| `LLMs/DeepSeek-Coder/shared/dspark-DeepSeek-V4.1-Flash-1017-Q8_0.gguf` | `~/Mounts/NVME/LLMs/DeepSeek-Coder/shared/` | 7.43 GiB | `6ba6ac1b331c540691a209e7fc97dec2a24e295acb1e755c84c7d24a4373927a` |
| `LLMs/DeepSeek-Coder/shared/UD-Q8_K_XL/…-00001-of-00009.gguf` | same | 5.8 MB | `cc15d470afcb030558bbc4d8e2ccd840182e2e95de1a95198138efe63a5fd99f` |
| `LLMs/DeepSeek-V4.1-Flash-Engram-IQ4_NL.gguf` | `~/Mounts/NVME/LLMs/` | 103.0 GiB | `3dac4c54893ad7aa89103c0028500eadc78e74112cef3a2930a97e67dc9fb4f3` |

## Restore (same paths the pack/config expect)

```sh
L=/home/garyb/Mounts/NVME/LLMs
A=/home/garyb/Mounts/RAID/Models/DeepSeek-V4.1-Flash-Coder-IQ2_XXS/LLMs
mkdir -p "$L/DeepSeek-Coder/shared"
mv "$A/DeepSeek-Coder/IQ2_XXS" "$L/DeepSeek-Coder/"
mv "$A/DeepSeek-Coder/shared/"* "$L/DeepSeek-Coder/shared/"
mv "$A/DeepSeek-Coder/mmproj-DeepSeek-V4.1-Flash-1017-BF16.gguf" "$L/DeepSeek-Coder/"
mv "$A/DeepSeek-V4.1-Flash-Engram-IQ4_NL.gguf" "$L/"
```

Then start it with `~/Mounts/NVME/LLMs/DeepSeek-Coder/run-dsv41.sh` (32K default) or `strata-deepseek-coder-1m.json`
for the 1M window. `--pack-info` should list all files present.
