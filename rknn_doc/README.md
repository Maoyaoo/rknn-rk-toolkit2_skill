# rknn_doc/ — Raw SDK documentation (V2.3.2)

This directory holds the **raw, authoritative** Rockchip RKNN SDK V2.3.2 documentation, extracted from the official PDFs to markdown via MinerU. It is the ground truth when the distilled guidance in `../references/` does not have enough detail.

## Two-layer architecture

The skill has two documentation layers:

| Layer | Location | Role | When to consult |
|---|---|---|---|
| **Distilled** | `references/*.md` | Cross-cutting, stable, opinionated guidance. Covers ~80% of cases. | Default — start here. |
| **Raw** | `rknn_doc/*.md` (this dir) | Per-op constraint tables, full API signatures, exact numeric limits. V2.3.2-pinned. | When you need a specific number, per-op constraint, or exact API signature not in `references/`. |

`references/` is curated and edited; `rknn_doc/` is verbatim from Rockchip. They overlap intentionally — `references/` summarises the patterns, `rknn_doc/` is the source of truth when a user challenges a summary or needs an edge case.

## Fast lookup patterns

Use `rg -n` and search the narrowest likely file first:

- Exact op support / limits: `rknn_doc/RKNNToolKit2_OP_Support-*.md` -> `rknn_doc/MinerU_markdown_05_*.md`
- Toolkit2 Python API: `rknn_doc/MinerU_markdown_03_*.md`
- RKNNRT C API / flags / query enums: `rknn_doc/MinerU_markdown_04_*.md`
- Conversion/runtime/troubleshooting narrative: `rknn_doc/MinerU_markdown_02_*.md`
- Board-side connected-debug issues: `rknn_doc/rknn_server_proxy.md`

Examples:

```bash
rg -n "Softmax|ConvTranspose|RMSNorm" rknn_doc/RKNNToolKit2_OP_Support-*.md
rg -n "Softmax|ConvTranspose|RMSNorm|kernel|stride|dilation" rknn_doc/MinerU_markdown_05_*.md
rg -n "accuracy_analysis|dynamic_input|op_target|codegen" rknn_doc/MinerU_markdown_03_*.md
rg -n "rknn_init_extend|rknn_create_mem2|RKNN_QUERY_" rknn_doc/MinerU_markdown_04_*.md
rg -n "RKNN_ERR_DEVICE_UNAVAILABLE|Invalid RKNN model version|NonMaxSuppression" \
  rknn_doc/MinerU_markdown_02_*.md rknn_doc/rknn_server_proxy.md
```

For a broader "question type -> file choice -> command -> follow-up" guide, see `../references/15_rknn_doc_lookup.md`.

## File map

| File | Source PDF | Use it for |
|---|---|---|
| `MinerU_markdown_01_Rockchip_RKNPU_Quick_Start_*.md` | RKNPU Quick Start | First-time setup, board connection, examples on RK356x/RK3576/RK3588/RV1126B |
| `MinerU_markdown_01_Rockchip_RV1106_RV1103_Quick_Start_*.md` | RV1106/RV1103 Quick Start | RV110x-specific: uClibc toolchain, `librknnmrt.so`, `/oem/usr/*` paths, kill `rkipc` |
| `MinerU_markdown_01_Rockchip_RK2118_Quick_Start_*.md` | RK2118 Quick Start | RT-Thread firmware build, serial debug (no adb), model deployment via FAT32 USB |
| `MinerU_markdown_02_Rockchip_RKNPU_User_Guide_*.md` | User Guide (342 KB) | The canonical reference — model conversion, evaluation, advanced features, quantization, accuracy triage, performance, memory, troubleshooting. **Best single source.** |
| `MinerU_markdown_03_Rockchip_RKNPU_API_Reference_RKNN_Toolkit2_*.md` | Toolkit2 Python API | `RKNN()` class methods, `config()` parameter table, `init_runtime()`, `accuracy_analysis()`, `hybrid_quantization_step1/2`, `codegen()`, `list_devices()`, `export_encrypted_rknn_model()` |
| `MinerU_markdown_04_Rockchip_RKNPU_API_Reference_RKNNRT_*.md` | RKNNRT C API | `rknn_init` flags, `rknn_query` commands, memory APIs, matmul API, custom op API, per-platform support matrix, error codes |
| `MinerU_markdown_05_RKNN_Compiler_Support_Operator_List_*.md` | Compiler op list (371 KB) | Per-operator numeric constraints (kernel size, stride, pad, dilation, dtype, shape limits) for every supported op × every SoC family. **Grep this when a specific op fails to compile or runs slowly.** |
| `rknn_server_proxy.md` | rknn_server proxy guide | Board-side `rknn_server` install, version checks, log levels, common connected-debug issues |
| `RKNNToolKit2_API_Difference_With_Toolkit1-*.md` | Toolkit1 → Toolkit2 API diff | Migrating from RKNPU1; new config knobs (`float_dtype`, `remove_weight`, `compress_weight`, `inputs_yuv_fmt`, `custom_string`) |
| `RKNNToolKit2_OP_Support-*.md` | Per-framework op support summary | Quick "is this ONNX/PyTorch/TF/Caffe/DarkNet op supported?" table — fast screen before opening the 371 KB compiler list |
| `Using RKNN-ToolKit2 in WSL.md` / `WSL中使用RKNN_ToolKit2.md` | WSL install guide | libGL fix, Ethernet adb, USB-to-TCP bridging |
| `Rockchip_Developer_Guide_RGA_CN.md` | RGA developer guide | low-level RGA API, formats, DMA buffers, performance (not RKNN-core; used by Model Zoo / zero-copy image paths) |
| `Rockchip_FAQ_RGA_CN.md` | RGA FAQ | common RGA driver/`librga.so` version issues, SEGV with JPEG/OpenCV, resize failures |

## Version pinning — read this before trusting any number

These docs are pinned to **RKNN SDK V2.3.2 (2025-04-03)**. Later releases may change:

- Per-op constraints (kernel limits, alignment rules, dtype support).
- API signatures (new params, deprecated flags).
- Platform support (new SoCs added, restrictions relaxed or tightened).
- Default behaviour.

**If the user's SDK is not V2.3.2**, treat the numbers in here as a hint and verify against their actual SDK's PDFs/headers before committing to a deployment decision. The cross-cutting guidance in `references/` is more version-stable than the per-op tables here.

To check the user's version:
- Toolkit2: `python -c "from rknn.api import RKNN; print(RKNN().__class__)"` or `pip show rknn-toolkit2`
- Runtime: `adb shell 'strings /usr/lib/librknnrt.so | grep -i "librknnrt version"'` or `rknn_get_sdk_version()` / `RKNN_QUERY_SDK_VERSION`

## License

These documents are © Rockchip Electronics Co., Ltd. and follow the RKNN SDK license terms — they are **not** covered by the MIT license that applies to the skill scaffolding (`../LICENSE`). Redistribution is permitted under Rockchip's public documentation terms; the SDK itself (runtime libraries, examples, header files) is **not** redistributed with this skill and remains under its own license.
