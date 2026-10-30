# Workflow: Convert model to RKNN

## Read first

1. `conventions/agent-response.md` + `references/01_platform_matrix.md` (Always Read)
2. `references/02_toolkit2_conversion.md`
3. `gotchas/high-cost.md` if target/lib/version is unclear

## Steps

0. If the user already named **onnx_edit / custom op / sparse_infer / AutoSparsity** as the main goal, prefer `workflows/graph-advanced.md` first (then return here for the final convert).
1. Confirm **source format** (ONNX / PyTorch / TF / TFLite / Caffe / DarkNet) and **target_platform** (lowercase).
2. Confirm host Toolkit2 arch and install health: x86 full frameworks vs ARM64 (ONNX + PyTorch only; no Docker; no adb connected-debug). If install/import is the blocker, use the host-environment section of `references/02_toolkit2_conversion.md` first.
3. Match preprocess: `mean_values` / `std_values`, layout, RGB/BGR, resize — same as training/export.
4. Decide quantization:
   - quant → need valid `dataset.txt` (`scripts/make_dataset_txt.py`)
   - FP16 / non-quant → `do_quantization=False`
   - dtype names: prefer V2.3.2 (`w8a8`, `w4a16`, …); verify legacy `asymmetric_quantized-8` against installed API
   - gate the dtype by SoC (`w4a16` RK3576/RV1126B, `w8a16` RK3562, `w16a16i*` RV1103/RV1106, no `w8a8`/`w4a8` on RK2118)
5. Start from the closest asset:
   - ONNX → `assets/convert_onnx_to_rknn.py`
   - PyTorch → `assets/convert_pytorch_to_rknn.py`
   - many similar models, no custom Python → `rknn_convert` YAML wrapper (`references/02_toolkit2_conversion.md`)
6. Build with verbose log; on op/build errors escalate via `workflows/operator-support.md` or `workflows/doc-lookup.md`.
   - Awkward I/O rank/layout → `workflows/graph-advanced.md` (`onnx_edit`).
   - Need custom op after rewrite fails → `workflows/graph-advanced.md` (`21_custom_op`).
7. Export `.rknn`; report next board step (Lite2 / C API / Model Zoo demo) if user will deploy.
   Optional C++ scaffold: `rknn.codegen()` (`references/02_toolkit2_conversion.md`).

## Done when

- Script/patch sets explicit `target_platform` and correct `load_*` API.
- Quant path has dataset guidance; preprocess contract stated.
- Answer passes `references/17_response_contracts.md` conversion set + `18_review_checklist.md`.
