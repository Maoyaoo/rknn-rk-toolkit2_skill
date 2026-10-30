# RKNN Answer Review Checklist

Use this file right before sending an RKNN/RKNPU2 answer. It is a final short self-check: not for gathering new context, but for catching the 1-2 missing details that would make the answer misleading.

## Core review questions

Before replying, check:

1. Did I name the target SoC or `target_platform` if it matters?
2. Did I identify the runtime mode correctly: conversion, simulator, connected inference, board runtime, Android app, or RT-Thread?
3. Did I name the correct runtime library family if deployment/runtime is involved?
4. Did I accidentally answer with the wrong API family for this platform?
5. Did I mention version consistency when `rknn_server` / runtime / Toolkit2 mismatch is a plausible cause?

If any answer is "no" or "not sure", the response is probably incomplete.

## Conversion review

- Did I state the exact `target_platform`?
- Did I preserve the preprocess contract (`mean/std`, layout, RGB/BGR, resize)?
- Did I state whether quantization needs `dataset.txt`?
- Did I avoid relying on Toolkit2 defaults where they are dangerous?
- If the issue could be framework-version or op-support related, did I point to the next verification path?
- If recommending custom op / onnx_edit / sparse_infer, did I gate on SoC and point to `21`/`22`/`23` instead of inventing APIs?
- Custom op: ONNX-only `reg_custom_op`, register-before-load, board register-after-init + `compute` required?
- Sparsity: stated RK3576/RV1126B for `sparse_infer`, and did not treat `model_pruning` as a densify-to-sparse magic switch?

## Quantization / accuracy review

- Did I separate preprocess/postprocess mismatch from quantization loss?
- Did I recommend the right comparison order (`framework -> FP simulator -> quant simulator -> runtime`)?
- Did I point to `accuracy_analysis` or equivalent first, instead of jumping to QAT?
- Did I state whether the likely issue is data, calibration, or graph/op behavior?

## Deployment / runtime review

- Did I name `librknnrt.so`, `librknnmrt.so`, or RK2118 mini runtime explicitly?
- Did I choose the right API path: general API vs zero-copy / mini-runtime?
- If RV110x or RK2118 is involved, did I explicitly avoid defaulting to `rknn_inputs_set` / `rknn_outputs_get`?
- Did I include at least one concrete path/library verification?

## `rknn_server` / connected-debug review

- Did I confirm the platform actually supports this flow?
- Did I mention Toolkit2 / `rknn_server` / runtime version alignment?
- Did I state the transport assumption correctly (`adb`, Ethernet adb, or no adb)?
- If the board is RK2118, did I explicitly say this flow does not apply?

## Troubleshooting review

- Did I identify the failure stage, not just the error string?
- Did I choose the smallest discriminating next check rather than dumping a long generic list?
- Did I separate these hypotheses when relevant:
  - wrong `target_platform`
  - version mismatch
  - wrong runtime library
  - unsupported op / constrained op
  - preprocess mismatch
  - memory pressure

## Operator support review

- Am I answering framework-level support, or exact compiler-level support?
- Did I mention that attrs/dtype/rank/SoC may change the answer?
- If I used raw docs, did I signal that the conclusion is V2.3.2-scoped unless re-verified?

## Advanced feature review

- Encryption: target SoC supported (not RV110x/RK2118), applied after `export_rknn`, and no password promise?
- Multi-batch: core count stated, batch size matched to cores, and zero-copy I/O recommended?
- SRAM: described as a BSP/device-tree reservation rather than a runtime call?

## Model Zoo / Android / RK2118 review

- Model Zoo:
  Did I preserve preprocess/postprocess/export assumptions instead of giving generic advice?

- Android:
  Did I mention ABI, packaging mode, and `librknnrt.so` path or HIDL/direct-link distinction if relevant?

- RK2118:
  Did I explicitly say RT-Thread, serial debug, mini runtime, and zero-copy style?

## Final stop-signs

Do not send the answer yet if any of these are true:

- I used "supported" but did not mention the constraint surface.
- I suggested deployment changes without naming the runtime library family.
- I discussed `rknn_server` for RK2118 as though it were normal Toolkit2 connected debug.
- I gave conversion guidance without naming `target_platform`.
- I gave C/C++ runtime guidance without checking platform API family.

## Fast pairing with other references

- Need to choose the task path first:
  `references/16_task_entry_map.md`
- Need raw-doc grep/escalation:
  `references/15_rknn_doc_lookup.md`
- Need the minimum answer shape for the task:
  `references/17_response_contracts.md`

This checklist is the final guard after those references, not a replacement for them.
