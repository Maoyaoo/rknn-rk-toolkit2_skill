# Workflow: Operator support / CPU fallback

## Read first

1. Always Read
2. `references/07_operator_support.md`
3. `references/15_rknn_doc_lookup.md`

## Steps

1. Never answer from ONNX Runtime / PyTorch support alone.
2. Collect: framework, op name, attrs (kernel/stride/pad/dilation), dtype, rank, quant mode, **target SoC**.
3. Fast screen: `rknn_doc/RKNNToolKit2_OP_Support-*.md`.
4. Exact limits: `rknn_doc/MinerU_markdown_05_*.md` (and narrative in User Guide if fallback/slow).
5. If "supported but slow": check CPU fallback / float32 path / layout constraints in User Guide + verbose build logs.
6. Mitigations in order:
   - graph rewrite / move fragile tail to host postprocess
   - I/O layout via `onnx_edit` (`workflows/graph-advanced.md` → `references/22_onnx_edit_and_graph_tools.md`)
   - `op_target` where documented
   - custom op **last** (`workflows/graph-advanced.md` → `references/21_custom_op.md`) — refuse on mini-runtime SoCs

## Done when

- Answer states support **with** SoC + attr caveats, or explicit unknown + what user must verify on their SDK version.
- If recommending custom op / onnx_edit / sparse, the matching advanced workflow ref is named.
