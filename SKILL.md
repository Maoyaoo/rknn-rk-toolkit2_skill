---
name: rknn-rk-toolkit2
description: "Rockchip RKNN / RKNPU2 (RKNN-Toolkit2 + Runtime) domain skill. Covers conversion to .rknn (ONNX/PyTorch/TensorFlow/TFLite/Caffe/DarkNet), quantization and accuracy triage (w8a8, w4a16, w8a16, w16a16i, hybrid, auto_hybrid, QAT), model encryption, RKNN Model Zoo porting, board-side C/C++ or RKNN-Toolkit-Lite2 Python inference, rknn_server / RKNN_ERR_DEVICE_UNAVAILABLE / Invalid RKNN model version debugging, operator-support checks, custom ops, onnx_edit, sparsity, dynamic shape, zero-copy, matmul, multi-batch, and performance/memory tuning on RK3562/RK3566/RK3568/RK3576/RK3588/RV1103/RV1103B/RV1106/RV1106B/RV1126B/RK2118. Use when the user says 转 rknn、量化掉精度、rknn_server 起不来、板端 C 推理、算子支不支持、自定义算子、onnx_edit、模型加密、Model Zoo 迁移、Lite2 板端 Python, or pastes an RKNN error. Not for RKNPU1 / RKNN-Toolkit v1."
---

# RKNN / RKNPU2 Skill

Domain skill for Rockchip RKNPU2 (Toolkit2 + Runtime). Routes tasks; does not dump full API tables.

## Always Read

1. `conventions/agent-response.md` — SoC/mode first, lookup order, response contract, hard platform rules
2. `references/01_platform_matrix.md` — RKNPU1 vs 2, target strings, runtime libs, host Toolkit differences

## Session discipline

- Re-match Common Tasks / `routing.yaml` on **every** new task in the session.
- Re-read route files only when the route changes or context was compacted.
- Prefer patch or runnable commands; default user-facing language: Chinese.
- Doc order: `references/` → `rknn_doc/` (via `references/15_rknn_doc_lookup.md`) → user's installed SDK/headers/PDFs only if version diverges.
- The package is self-contained. Never assume an external SDK checkout is present; describe example paths relative to the user's own Toolkit2 / `rknpu2` / `rknn_model_zoo` install.

## Common Tasks

Source of truth: `routing.yaml`. Match one row, read required files + workflow, then act.

| Task | Workflow | Read after Always Read |
|---|---|---|
| Toolkit2 install / host env / import error | `workflows/convert-model.md` | `references/02_toolkit2_conversion.md` (host environment), `08_wsl.md` for WSL |
| Convert / export `.rknn` | `workflows/convert-model.md` | `references/02_toolkit2_conversion.md` |
| Quantization / accuracy | `workflows/quant-accuracy.md` | `references/03_quantization_accuracy.md`, `02_toolkit2_conversion.md` |
| rknn_server / init / device errors | `workflows/board-debug.md` | `04_board_debug_rknn_server.md`, `09_troubleshooting.md`, `gotchas/high-cost.md` |
| Board C/C++ inference | `workflows/runtime-c.md` | `references/05_runtime_c_api.md` (+ `11_*` if dynamic/mem) |
| Operator support / fallback | `workflows/operator-support.md` | `07_operator_support.md`, `15_rknn_doc_lookup.md` |
| Custom op / onnx_edit / sparsity | `workflows/graph-advanced.md` | `21_custom_op.md` / `22_onnx_edit_and_graph_tools.md` / `23_sparsity_and_toolkit_functions.md` |
| Model Zoo port / demo adapt | `workflows/model-zoo-port.md` | `10_model_zoo_workflows.md`, `09_troubleshooting.md` |
| Platform special (Lite2 / Android / RV110x / RK2118 / WSL / perf / encryption / multi-batch) | `workflows/platform-special.md` | branch table inside workflow |
| Exact API / op limit / raw error | `workflows/doc-lookup.md` | `15_rknn_doc_lookup.md`, `00_source_map.md` |
| Unlisted | `references/16_task_entry_map.md` | closest workflow above |

## Known gotchas (activation)

Full text: `gotchas/high-cost.md`. Highest cost:

- Wrong lib / API family (`librknnrt` vs `librknnmrt`; general API on RV110x/RK2118)
- Toolkit2 / `rknn_server` / Runtime / model version skew
- Missing `target_platform`; RV110x ↔ B-variant model not interchangeable
- Preprocess/postprocess contract drift; thin calibration set
- Claiming op support without SoC + attr check in `rknn_doc/`
- Custom op / `sparse_infer` on wrong SoC or after `load_onnx`; "no zoo demo" ≠ unsupported
- Unsupported dtype gates (`w8a8` off on RK2118; `w4a16` RK3576/RV1126B; `w8a16` RK3562; `w16a16i*` RV1103/RV1106; `w4a8` not supported anywhere; `float_dtype` is float16-only)
- `onnx_edit` only fixes external I/O layout — not mid-graph op deletion; encryption unsupported on RV110x/RK2118

## Two-layer docs

| Layer | Path | Use |
|---|---|---|
| Distilled | `references/*.md` | default patterns (~80%) |
| Raw V2.3.2 | `rknn_doc/*.md` | numbers, signatures, per-op limits |

Escalation and grep recipes: `references/15_rknn_doc_lookup.md`. File map: `rknn_doc/README.md`, `references/00_source_map.md`.

## Tools

- Scripts: `scripts/rknn_env_probe.py`, `make_dataset_txt.py`, `rknn_server_commands.py`, `smoke_test.sh`
- Templates: `assets/convert_*.py`, `assets/accuracy_analysis_template.py`, `assets/lite2_python_infer.py`, `assets/c_api_*.c(pp)`, `assets/CMakeLists.rknn_template.txt`

## Core principles

1. **SoC + mode before code** — wrong platform choice invalidates the whole answer. ✓ Check: answer names SoC (or inference source) and runtime mode?
2. **API family matches SoC** — mini-runtime boards never get default `rknn_inputs_set` paths. ✓ Check: lib name + API family stated for deploy tasks?
3. **Distilled then raw** — do not guess limits; open `rknn_doc/` before "unknown". ✓ Check: unresolved number/API/error searched in raw docs?
4. **Version-pin honesty** — packaged raw docs are V2.3.2. ✓ Check: non-2.3.2 SDK told to re-verify?
5. **Response contract** — `references/17_response_contracts.md` + `18_review_checklist.md` before send. ✓ Check: checklist run?
