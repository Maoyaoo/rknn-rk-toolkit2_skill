# RKNN Raw-Doc Lookup Playbook

Use this file when `references/*.md` narrows the direction but does not answer the exact RKNN question. The goal is to get from a vague blocker to the smallest relevant `rknn_doc/*.md` source quickly, without grepping the whole raw-doc tree blindly.

## Escalation rule

Escalate from `references/` to `rknn_doc/` when you still need one of these:

- exact per-op numeric limits
- exact Toolkit2 / RKNNRT API parameters or enum names
- exact error-message root cause
- SoC-specific support differences
- version-gated behavior that the distilled layer only summarizes

Do not ask the user for low-level RKNN details until you have tried the raw-doc lookup path first.

## Search workflow

1. Start with the narrowest likely raw doc.
2. Search the longest distinctive token first: full error string, exact API name, exact op name, exact SoC name.
3. If there is no hit, shorten to the stable stem: function family, op family, subsystem term.
4. Once you find a hit, read enough nearby context to extract the rule, not just the matching line.
5. If the user's SDK is not V2.3.2, report the result as a V2.3.2 reference and state what must be re-verified in the user's actual SDK.

## Quick routing table

| Question type | First file to search | Then widen to |
|---|---|---|
| Is this framework op supported at all? | `rknn_doc/RKNNToolKit2_OP_Support-*.md` | `rknn_doc/MinerU_markdown_05_*.md` |
| What are the exact op limits on this SoC? | `rknn_doc/MinerU_markdown_05_*.md` | `rknn_doc/MinerU_markdown_02_*.md` |
| What does this Toolkit2 Python API accept? | `rknn_doc/MinerU_markdown_03_*.md` | `rknn_doc/RKNNToolKit2_API_Difference_With_Toolkit1-*.md` |
| `rknn_convert` YAML fields / CLI flags | `rknn_doc/MinerU_markdown_02_*.md` §3.1.6 | `MinerU_markdown_03_*.md` `config()` table |
| What does this RKNNRT C API / flag / query enum mean? | `rknn_doc/MinerU_markdown_04_*.md` | `rknn_doc/MinerU_markdown_02_*.md` |
| Why did conversion/build fail? | `rknn_doc/MinerU_markdown_02_*.md` | `rknn_doc/MinerU_markdown_05_*.md` |
| Why does connected debug / rknn_server fail? | `rknn_doc/rknn_server_proxy.md` | `rknn_doc/MinerU_markdown_01_*.md` |
| What is different on RK2118 / RV110x / RK3576? | matching `MinerU_markdown_01_*_Quick_Start_*.md` | `rknn_doc/MinerU_markdown_04_*.md` |
| RGA resize / DMA / `librga` version / JPEG+OpenCV SEGV | `rknn_doc/Rockchip_FAQ_RGA_CN.md` | `rknn_doc/Rockchip_Developer_Guide_RGA_CN.md` |
| Custom op / `reg_custom_op` / `rknn_register_custom_ops` | distilled `references/21_custom_op.md` first | `MinerU_markdown_03_*.md` (Toolkit2 § custom), `MinerU_markdown_04_*.md` + installed `rknn_custom_op.h` |
| `onnx_edit` transform equations | distilled `references/22_onnx_edit_and_graph_tools.md` | `MinerU_markdown_02_*.md` User Guide onnx_edit / graph opt chapters |
| `sparse_infer` / AutoSparsity / `model_pruning` | distilled `references/23_sparsity_and_toolkit_functions.md` | `MinerU_markdown_03_*.md` `config()` table; `MinerU_markdown_02_*.md` sparse chapter |

## Copy-paste search recipes

### 1. Operator support and numeric limits

Fast screen:

```bash
rg -n "Softmax|AveragePool|ConvTranspose|GridSample|LayerNorm|RMSNorm|FlashAttention" \
  rknn_doc/RKNNToolKit2_OP_Support-*.md
```

Exact constraints:

```bash
rg -n "Softmax|AveragePool|ConvTranspose|GridSample|LayerNorm|RMSNorm|FlashAttention|kernel|stride|pad|dilation" \
  rknn_doc/MinerU_markdown_05_*.md
```

Use this when the user asks:

- "这个 op 到底支不支持？"
- "为什么同一个 op 在 RK3588 可以，在 RK2118 不行？"
- "kernel/stride/dilation 上限是多少？"

### 2. Toolkit2 Python API details

```bash
rg -n "accuracy_analysis|dynamic_input|op_target|fallback_prior_device|codegen|load_rknn|list_devices" \
  rknn_doc/MinerU_markdown_03_*.md
```

Use this when the user asks:

- "`init_runtime()` 这个参数到底什么意思？"
- "`load_rknn()` 后还能不能 `accuracy_analysis()`？"
- "`op_target` / `dynamic_input` / `codegen` 怎么用？"

### 3. RKNNRT C API, flags, queries, runtime memory

```bash
rg -n "rknn_init_extend|rknn_create_mem2|rknn_set_batch_core_num|rknn_set_core_mask|RKNN_QUERY_|RKNN_FLAG_" \
  rknn_doc/MinerU_markdown_04_*.md
```

Use this when the user asks:

- "`RKNN_FLAG_MEM_ALLOC_OUTSIDE` 具体怎么配？"
- "`RKNN_QUERY_CURRENT_INPUT_ATTR` / `RKNN_QUERY_MEM_SIZE` 适用于哪些平台？"
- "`rknn_set_core_mask` 和 `rknn_set_batch_core_num` 区别是什么？"

### 4. Conversion/build/runtime error messages

```bash
rg -n "RKNN_ERR_DEVICE_UNAVAILABLE|Invalid RKNN model version|vector::_M_range_check|failed to allocate fd|NonMaxSuppression|dynamic graph" \
  rknn_doc/MinerU_markdown_02_*.md rknn_doc/rknn_server_proxy.md
```

Shorten when needed:

```bash
rg -n "DEVICE_UNAVAILABLE|model version|allocate fd|NonMaxSuppression" \
  rknn_doc/MinerU_markdown_02_*.md rknn_doc/rknn_server_proxy.md
```

Use this when the user pastes a log line and the distilled docs do not already explain it.

### 5. SoC-specific differences and deployment path

```bash
rg -n "RK2118|RV1106B|RV1103B|RV1126B|librknnmrt|mini runtime|rknn_server|dynamic shape" \
  rknn_doc/*.md
```

Then narrow to the relevant quick start:

```bash
rg -n "RK2118|serial|RT-Thread|SYSTEM DEBUG" \
  rknn_doc/MinerU_markdown_01_Rockchip_RK2118_Quick_Start_*.md
rg -n "RV1106|RV1103|librknnmrt|rkipc|/oem/usr" \
  rknn_doc/MinerU_markdown_01_Rockchip_RV1106_RV1103_Quick_Start_*.md
```

Use this when the question is really about platform path selection, not generic RKNN usage.

## Common lookup mistakes

- Searching all raw docs immediately for a tiny token like `shape` or `perf`.
- Stopping at `RKNNToolKit2_OP_Support-*.md` without confirming attribute limits in `MinerU_markdown_05_*.md`.
- Answering from one matching line without reading the surrounding paragraph/table.
- Forgetting to include the SoC in the final answer when the raw doc rule is platform-dependent.
- Reporting V2.3.2 raw-doc numbers as universal truth when the user's installed SDK may differ.

## Response pattern after a raw-doc hit

When you answer based on `rknn_doc/`, include:

1. the file family used (`MinerU_markdown_03_*`, `MinerU_markdown_04_*`, etc.)
2. whether the result is an exact API rule, an op constraint, or a troubleshooting note
3. whether the conclusion is V2.3.2-specific and what must be re-verified on a newer/older SDK

That keeps the answer concrete without pretending the raw doc is version-agnostic.
