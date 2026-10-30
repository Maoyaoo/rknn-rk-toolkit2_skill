# High-Cost Gotchas

Costly mistakes that repeatedly burn hours. Each entry should change the next action when read. Full playbooks stay in `references/`; this file is the short activation layer.

## 1. Wrong runtime library / mini-runtime API

**Symptom:** board load fails, link wrong, or demo works only after swapping `.so`; or code uses `rknn_inputs_set` on RV110x/RK2118.

**Cause:** mixing `librknnrt.so` with `librknnmrt.so`, or treating mini-runtime SoCs as general-API platforms.

**Next action:**
- RK3562/RK3566/RK3568/RK3576/RK3588/RV1126B → `librknnrt.so` + general API allowed.
- RV1103/RV1106 (+B) → `librknnmrt.so` + zero-copy / `rknn_set_io_mem`.
- RK2118 → mini-runtime from RT-Thread SDK; serial deploy, no adb/`rknn_server` path.
- Re-read `references/01_platform_matrix.md` + `references/05_runtime_c_api.md`.

## 2. Toolkit2 / rknn_server / Runtime version skew

**Symptom:** `RKNN_ERR_DEVICE_UNAVAILABLE`, `Invalid RKNN model version`, model loads on PC simulator but not on board.

**Cause:** model built with newer Toolkit2 than board runtime, or `rknn_server` not updated with runtime.

**Next action:** print Toolkit2, server, and `.so` versions; align all three; rebuild model or update board stack. Use `scripts/rknn_env_probe.py` + `references/04_board_debug_rknn_server.md`. On RV110x, `RKNN_ERR_MODEL_INVALID` at init can also be memory pressure from `rkipc`, not a version problem — check `references/06_performance_memory.md` before rebuilding.

## 3. Missing or wrong `target_platform`

**Symptom:** model only runs on RK3566/RK3568, or wrong SoC at init.

**Cause:** omitted `rknn.config(target_platform=...)` (Toolkit2 defaults to RK3566-class).

**Next action:** set lowercase target explicitly (`rk3588`, `rv1106`, `rv1126b`, …). B-variants need their own builds (`rv1106` ≠ `rv1106b`).

## 4. Preprocess / postprocess contract drift

**Symptom:** Model Zoo demo OK, user model wrong boxes/classes; confidences > 1; boxes fill image.

**Cause:** mean/std, RGB/BGR, NHWC/NCHW, letterbox color, anchors, sigmoid/NMS, or dequant mismatch vs training/export.

**Next action:** lock the original export preprocess first; do not “fix” quantization until FP path matches. See `references/03_quantization_accuracy.md` + `references/10_model_zoo_workflows.md`.

## 5. Calibration dataset too thin or unrepresentative

**Symptom:** int8 accuracy collapse while FP16 simulator looks fine.

**Cause:** few images, wrong domain, or wrong multi-input dataset format.

**Next action:** rebuild `dataset.txt` (use `scripts/make_dataset_txt.py`); start ~20–100 normal/KL, 20–50 MMSE, more for gdq/w4a16; then `accuracy_analysis` before hybrid/QAT.

## 6. Claiming op support without SoC/attr check

**Symptom:** compile fail, silent CPU fallback, or slow “supported” model.

**Cause:** ONNX Runtime support ≠ RKNN NPU support; attrs (kernel/stride/dilation/rank/dtype) matter.

**Next action:** `references/07_operator_support.md` → `rknn_doc/RKNNToolKit2_OP_Support-*.md` → `MinerU_markdown_05_*.md`. Never assert support from framework docs alone.

## 7. Multi-core API on wrong SoC

**Symptom:** invalid core mask, no speedup, or runtime error on RK3576.

**Cause:** copying RK3588 core-mask examples onto RK3576 (2 cores) or single-core SoCs.

**Next action:** RK3588 three cores; RK3576 two — do not select core 2 or 0_1_2 on RK3576. Use `rknn_set_batch_core_num` only for multi-batch models on RK3588/RK3576; it is not a replacement for `rknn_set_core_mask`. See `references/06_performance_memory.md`.

## 8. RV1126B is not mini-runtime

**Symptom:** wrong path under `armhf-uclibc` or treating RV1126B like RV1106.

**Cause:** naming similarity to RV110x; docs sometimes group driver query paths poorly.

**Next action:** RV1126B uses `librknnrt.so` and normal runtime layout; driver version often via `/proc/rknpu/version` (with RV110x), **not** the mini-runtime library path. See `references/04_board_debug_rknn_server.md`.

## 9. Custom op / sparse_infer on the wrong SoC or stage

**Symptom:** convert fails on `reg_custom_op`; board cannot find op; or `sparse_infer` does nothing / errors on dense RK3588 models.

**Cause:** mini-runtime has no custom-op path; `sparse_infer` is **RK3576/RV1126B only**; Toolkit2 `reg_custom_op` is **ONNX-only** and must run **before** `load_onnx`; board C must `rknn_register_custom_ops` **after** `rknn_init` with a `compute` callback.

**Next action:** open `references/21_custom_op.md` or `23_sparsity_and_toolkit_functions.md`; verify SoC; never enable sparse knobs on dense non-gated SoCs; match `op_type` across ONNX / Python / C.

## 10. "No Model Zoo demo" ≠ unsupported

**Symptom:** agent refuses a network because zoo has no folder for it.

**Cause:** zoo is a curated sample set (FAQ 1.8), not an allow-list.

**Next action:** open `workflows/model-zoo-port.md` step 0; convert with Toolkit2; check op support; borrow closest-family pre/postprocess only. Large-model demos are also absent (FAQ 1.9) — do not invent LLM zoo paths.

## 11. onnx_edit is not middle-op surgery

**Symptom:** user asks to "onnx_edit away" Softmax/NMS/custom logic in the middle of the graph.

**Cause:** `rknn.utils.onnx_edit` rewrites **external input/output** reshape/transpose for graph opt, not arbitrary mid-graph op replacement.

**Next action:** `references/22_onnx_edit_and_graph_tools.md` for I/O equations; mid-graph → rewrite / postprocess / `references/21_custom_op.md`.

## 12. Unsupported dtype knobs on the target SoC / SDK

**Symptom:** `w8a8` build fails or silently produces an FP16-only path on RK2118; `float_dtype='float32'` is rejected or ignored.

**Cause:** In V2.3.2, `w8a8` is **not supported on RK2118**, `w4a8` is listed but **not implemented anywhere**, and `float_dtype` accepts only `'float16'`. `w4a16` is RK3576/RV1126B-only; `w8a16` is RK3562-only; `w16a16i*` is RV1103/RV1106-only; `gdq` only works with `w4a16`/`w8a16`.

**Next action:** check the `config()` dtype table in `references/02_toolkit2_conversion.md` + `references/03_quantization_accuracy.md`; use an FP16 build on RK2118.

## 13. Encryption or multi-batch promised on the wrong target

**Symptom:** `export_encrypted_rknn_model` fails or produces a model the board cannot load; or a multi-batch build shows no speedup.

**Cause:** Model encryption is **not supported on RV1103/RV1103B/RV1106/RV1106B/RK2118** in V2.3.2. Multi-batch only parallelizes on multi-core NPUs (RK3588 3 cores, RK3576 2 cores) and only pays off with the zero-copy interface.

**Next action:** confirm SoC before promising either feature; see `references/13_advanced_features.md`.
