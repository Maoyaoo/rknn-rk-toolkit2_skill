# Workflow: Graph tools / custom op / sparsity

## Read first

1. Always Read
2. Branch by signal (open **one** primary ref first):

| Signal | Primary read | Also |
|---|---|---|
| custom op / `reg_custom_op` / `rknn_custom_op` / non-standard ONNX op | `references/21_custom_op.md` | `07_operator_support.md`, `05_runtime_c_api.md` |
| onnx_edit / I/O reshape-transpose / 5D→4D for NPU | `references/22_onnx_edit_and_graph_tools.md` | `02_toolkit2_conversion.md` |
| sparse / AutoSparsity / `sparse_infer` / `model_pruning` | `references/23_sparsity_and_toolkit_functions.md` | `02_toolkit2_conversion.md` |
| which Toolkit2 `examples/functions` demo? | `references/22_onnx_edit_and_graph_tools.md` | `20_official_examples_and_smoke_paths.md` |
| hybrid quant steps only | `workflows/quant-accuracy.md` + `03` | `22` hybrid order note |

## Steps

1. Prefer **graph rewrite / host postprocess / onnx_edit** before custom op.
2. **Hard refuses (state explicitly, do not invent workarounds):**
   - custom op on RV110x / RK2118 mini-runtime
   - `sparse_infer` on anything other than **RK3576 / RV1126B**
   - `sparse_infer` / AutoSparsity on a **dense** model with no sparse training; `model_pruning` on a dense model is a no-op
   - Toolkit2 `reg_custom_op` without an **ONNX** graph
   - `onnx_edit` on mid-graph ops; dynamic-shape transforms only if the installed Toolkit2 accepts them
3. Point examples at the **user's SDK install** tree, never at paths from another SDK version.
4. After graph change: re-run conversion + accuracy path (`workflows/convert-model.md` / `quant-accuracy.md`).
5. Board custom C: only after Toolkit2 registration produces a valid `.rknn`; use field quick-ref in `references/21_custom_op.md`.

## Done when

- Chosen tool matches SoC + problem class (or a clear refuse with the gate cited).
- Next concrete command or SDK example name is given.
