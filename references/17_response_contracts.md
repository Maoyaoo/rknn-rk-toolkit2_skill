# RKNN Response Contracts

Use this file when answering RKNN/RKNPU2 requests and you want to avoid omitting the one or two details that make an otherwise-correct answer unusable.

The rule is simple: for each task type, include the minimum information set below unless the user explicitly asked for something narrower.

## General contract

Almost every RKNN answer should make these points explicit when they matter:

- target SoC or `target_platform`
- runtime mode: conversion host, connected inference, board runtime, Android app, RT-Thread, etc.
- relevant runtime library family: `librknnrt.so`, `librknnmrt.so`, or RK2118 mini runtime
- version consistency risk when Toolkit2 / `rknn_server` / runtime may mismatch

Do not bury these points in optional caveats. They are part of the answer, not background trivia.

## Task-specific minimum answer sets

### 1. Conversion answers

Must include:

- explicit `target_platform`
- whether quantization is involved
- preprocess contract to preserve (`mean/std`, layout, RGB/BGR, resize)
- whether a dataset file is required
- the next concrete artifact or command (`build`, `export_rknn`, script/template path)

Good minimum shape:

1. target board/SoC
2. exact conversion path (`load_onnx`, `load_pytorch`, etc.)
3. preprocessing constraints
4. quantization requirement
5. runnable command/script or patch direction

### 2. Quantization / accuracy answers

Must include:

- whether the issue is FP16-vs-quantized divergence or postprocess mismatch
- the comparison order (`framework -> FP simulator -> quant simulator -> runtime`)
- the first recommended verification tool (`accuracy_analysis`, representative sample, dataset quality)
- whether the likely problem is data/preprocess, calibration, or graph/op behavior

Do not jump straight to "use QAT" without covering PTQ triage first.

### 3. Board deployment / runtime answers

Must include:

- target runtime family (`librknnrt.so` vs `librknnmrt.so` vs RK2118 mini runtime)
- which API path is valid (general API vs zero-copy / `rknn_set_io_mem`)
- board OS / deployment style if relevant (Android, Linux, RT-Thread)
- one concrete library/path check

If the target is RV110x or RK2118, explicitly say that the general `rknn_inputs_set` / `rknn_outputs_get` path is not the default answer.

### 4. `rknn_server` / connected-debug answers

Must include:

- whether the flow even applies on that platform
- version consistency check across Toolkit2 / `rknn_server` / runtime
- transport assumption (`adb`, Ethernet adb, no adb on RK2118)
- one concrete verification command

If the platform is RK2118, explicitly say there is no regular Toolkit2 connected-debug path.

### 5. Troubleshooting answers

Must include:

- exact failure stage: conversion, simulator, connected inference, board runtime
- the most likely mismatch axis: SoC target, version, runtime path, unsupported op, preprocess mismatch, memory pressure
- the first discriminating check to run next
- if applicable, the likely file/doc to consult next

Do not give a flat list of ten checks when one or two checks will separate the major hypotheses.

### 6. Operator support answers

Must include:

- whether the answer is framework-level support or exact compiler-level support
- whether attrs/dtype/rank/SoC can change the answer
- if exact constraints matter, that the final answer comes from raw docs and is V2.3.2-scoped unless re-verified

Do not answer "supported" without noting the constraint surface when it matters.

### 6b. Custom op / onnx_edit / sparsity answers

Must include:

- **Custom op:** SoC family (full Runtime only); ONNX-only Toolkit2 registration; register-before-load; board `rknn_register_custom_ops` after `rknn_init`; backend CPU vs GPU; that rewrite/postprocess/`onnx_edit` was considered first
- **onnx_edit:** that it adjusts **external** I/O reshape/transpose for graph opt — not arbitrary middle-op replacement; equation rules or pointer to `references/22_onnx_edit_and_graph_tools.md`
- **Sparsity:** distinguish AutoSparsity training vs `sparse_infer` vs `model_pruning`; state SoC gate (`sparse_infer` → RK3576/RV1126B); never imply dense models gain from `sparse_infer` alone

Point at `workflows/graph-advanced.md` + `21`/`22`/`23`. Do not invent C struct fields without raw/header check.

### 6c. Encryption / multi-batch / SRAM answers

Must include:

- **Encryption:** whether the target SoC is in the supported list (not RV110x/RK2118); that it happens after `export_rknn`; that no password option exists in V2.3.2
- **Multi-batch:** core count of the target (RK3588 = 3, RK3576 = 2, others single-core), that batch size should match the core count, and that zero-copy I/O is required for the throughput win
- **NPU SRAM:** that reservation is a BSP/device-tree decision, not an app API; query `rknn_mem_size` for `total_sram_size` / `free_sram_size` (RK3588 only)

### 7. Model Zoo porting answers

Must include:

- closest demo family or workflow
- preprocess/postprocess contract that must be preserved
- whether export graph changes are likely required
- whether the issue is in conversion, decode, anchors, sigmoid, or platform runtime

Do not answer with a generic "replace the model file" style suggestion.

### 8. Android deployment answers

Must include:

- ABI/packaging target
- `librknnrt.so` path or HIDL/direct-link mode when relevant
- whether `rknn_server` connected debug is part of the workflow
- one packaging/runtime pitfall to verify next

### 9. RK2118 answers

Must include:

- RT-Thread, not Linux/Android
- serial debug, not adb
- RK2118 mini runtime path, not normal `rknpu2/runtime`
- zero-copy/mini-runtime style, not generic general API

If these points are missing, the answer is very likely misleading.

## Response patterns by task shape

### When the user asks for a fix

Answer in this order:

1. likely root cause
2. what to change
3. why that RKNN-specific choice is necessary
4. what to verify next

### When the user asks for code

Answer in this order:

1. correct API family for the target
2. template or file to start from
3. target-specific constraint
4. any required runtime/library note

### When the user asks "why"

Answer in this order:

1. direct cause
2. where the rule comes from (distilled or raw-doc layer)
3. what evidence/check would confirm it

## Common answer failures

- Suggesting a conversion patch without naming `target_platform`
- Giving deployment advice without naming the runtime library family
- Discussing `rknn_server` as if it applies to RK2118
- Saying "unsupported op" when the real answer is "supported with attribute limits"
- Giving a generic accuracy answer without separating preprocess mismatch from quantization damage
- Giving a generic runtime answer without separating general API from mini-runtime / zero-copy paths
- Enabling `sparse_infer` or custom ops without SoC/API-family gates
- Treating missing Model Zoo demos as "network unsupported"

Before sending the answer, run `references/18_review_checklist.md` as a last pass.
