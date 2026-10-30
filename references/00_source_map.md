# Source Map

This skill has two documentation layers:

- **`references/*.md`** (this directory) — distilled, cross-cutting guidance covering ~80% of cases. Curated and edited.
- **`rknn_doc/*.md`** — the raw V2.3.2 SDK docs in markdown form, shipped with the skill as the authoritative ground truth for per-op constraints, exact API signatures, and numeric limits that don't fit in the distilled layer. See `rknn_doc/README.md` for the file→topic map.

Agent lookup rule:

1. Start with `references/`.
   If the user's ask is still broad ("帮我转模型", "帮我排查板端问题", "给我 C 推理代码"), route first with `references/16_task_entry_map.md`.
2. If `references/` does not fully answer the question, escalate to `rknn_doc/` before guessing or asking the user for low-level RKNN details.
3. The package is self-contained. If both layers miss, consult the user's installed SDK headers/PDFs; do not assume any external checkout exists.

When the agent knows it needs `rknn_doc/` but is unsure which raw file to search first, use `references/15_rknn_doc_lookup.md`.
When the agent knows the technical direction but is about to answer the user, use `references/17_response_contracts.md` to avoid omitting RKNN-specific prerequisites or compatibility notes.
Right before sending the answer, use `references/18_review_checklist.md` as the final short self-check.

`references/15`–`18` are meta-guides (lookup, routing, contracts, checklist); they deliberately contain no RKNN facts of their own. If an SDK release changes, refresh them only when the *doc set* changes, not when a number changes. Maintainer procedure: `references/24_maintainer_update.md`.

## Documents behind the distilled layer

Every fact in `references/` traces back to one of these source documents (all V2.3.2, all available raw in `rknn_doc/`):

- `01_Rockchip_RKNPU_Quick_Start_RKNN_SDK_V2.3.2_CN.pdf` / EN — `rknn_doc/MinerU_markdown_01_Rockchip_RKNPU_Quick_Start_*.md`
- `01_Rockchip_RV1106_RV1103_Quick_Start_RKNN_SDK_V2.3.2_CN.pdf` / EN — `rknn_doc/MinerU_markdown_01_Rockchip_RV1106_RV1103_Quick_Start_*.md`. Source for the RV110x B-variant model incompatibility note in `references/01_platform_matrix.md`, the rkipc-kill memory note in `references/06_performance_memory.md`, and the matching troubleshooting entries in `references/09_troubleshooting.md`.
- `01_Rockchip_RK2118_Quick_Start_RKNN_SDK_V2.3.2_CN.pdf` / EN — `rknn_doc/MinerU_markdown_01_Rockchip_RK2118_Quick_Start_*.md`. Sole source for `references/14_rk2118_rtos_deploy.md` (RT-Thread, serial debug, no adb, runtime origin).
- `02_Rockchip_RKNPU_User_Guide_RKNN_SDK_V2.3.2_CN.pdf` / EN — `rknn_doc/MinerU_markdown_02_Rockchip_RKNPU_User_Guide_*.md`. Primary source for `references/13_advanced_features.md` (chapters 5.6 multi-batch, 5.9 encryption, 5.10 cacheable sync, 5.11 sparse, 5.13 onnx_edit, 6.4 QAT, 9.5 shared weights).
- `03_Rockchip_RKNPU_API_Reference_RKNN_Toolkit2_V2.3.2_CN.pdf` / EN — `rknn_doc/MinerU_markdown_03_*_Toolkit2_*.md`. Source for the `config()` parameter table, `init_runtime()` extras (`async_mode`, `fallback_prior_device`), `load_rknn()` constraint, `get_sdk_version()` 3-line output, `list_devices()`, `codegen()` in `references/02_toolkit2_conversion.md`.
- `04_Rockchip_RKNPU_API_Reference_RKNNRT_V2.3.2_CN.pdf` / EN — `rknn_doc/MinerU_markdown_04_*_RKNNRT_*.md`. Source for the `rknn_init` flag table, native attr query variants, per-platform C API support matrix, `rknn_set_batch_core_num` vs `set_core_mask`, `rknn_create_mem2`, error code table in `references/05_runtime_c_api.md` and `references/04_board_debug_rknn_server.md`. Source for the Android HIDL vs direct-link distinction in `references/12_android_deploy.md`.
- `05_RKNN_Compiler_Support_Operator_List_V2.3.2.pdf` — `rknn_doc/MinerU_markdown_05_*.md`. Source for the cross-cutting operator constraints in `references/07_operator_support.md` (Conv kernel/stride/pad/dilation limits, first-layer width × dilation cap table, Softmax product caps, RNN batch%4 rule, 4D cap, platform-gated fused ops) and the input/output alignment tables in `references/05_runtime_c_api.md`.
- `rknn_server_proxy.md` — `rknn_doc/rknn_server_proxy.md`. Source for the Android SELinux tip in `references/12_android_deploy.md`.
- `RKNNToolKit2_API_Difference_With_Toolkit1-2.3.2.md` — `rknn_doc/RKNNToolKit2_API_Difference_With_Toolkit1-2.3.2.md`. Source for the new config knobs (`float_dtype`, `remove_weight`, `compress_weight`, `inputs_yuv_fmt`, `custom_string`) in `references/02_toolkit2_conversion.md`.
- `RKNNToolKit2_OP_Support-2.3.2.md` — `rknn_doc/RKNNToolKit2_OP_Support-2.3.2.md`. Fast per-framework op screen before opening the full compiler list.
- `Using RKNN-ToolKit2 in WSL.md` / `WSL中使用RKNN_ToolKit2.md` — `rknn_doc/`.
- `Rockchip_Developer_Guide_RGA_CN.md` / `Rockchip_FAQ_RGA_CN.md` — `rknn_doc/`. Source for RGA driver/`librga` version notes in `references/09_troubleshooting.md`, Android RGA requirements in `references/12_android_deploy.md`, and zero-copy image pipeline caveats in `references/05_runtime_c_api.md` / `assets/c_api_rga_zero_copy_template.cpp`. Not part of the core RKNN API surface; packaged because Model Zoo demos depend on RGA.

## Upstream sources behind the distilled layer

The skill was distilled from Rockchip's official repositories. They are not redistributed with the skill; obtain them from Rockchip's official release when you need raw examples, headers, or libraries:

- `rknn-toolkit2` (a.k.a. `rknpu2`): RKNN SDK V2.3.2 docs, Toolkit2 examples, Lite2 examples, Runtime C API examples, and `rknn_server_proxy.md`.
- `rknn_model_zoo`: model conversion scripts, Python inference demos, C/C++ Runtime demos, build scripts, common utilities, FAQ, and compilation guides.

All example paths in this skill are described relative to the user's own SDK / Model Zoo install. No external checkout is part of the package.

## Scope

The source version is RKNN SDK / RKNN-Toolkit2 V2.3.2 (2025-04-03). If the user has a different SDK release:

- The cross-cutting guidance in `references/` is reasonably version-stable.
- The per-op tables and exact API signatures in `rknn_doc/` are V2.3.2-pinned — treat them as hints and verify against the user's actual SDK PDFs/headers before committing to a deployment decision.

## Practical workflow references distilled from official SDK examples

These distilled references are based primarily on `rknn_doc/`, but also fill practical workflow gaps from the official Toolkit2 / rknpu2 / Model Zoo examples:

- `references/19_toolkit_lite2_board_python.md` — distilled from the Lite2 package/examples and Model Zoo FAQ board-side Python guidance.
- `references/20_official_examples_and_smoke_paths.md` — distilled from the official `rknpu2/examples` smoke paths (`rknn_common_test`, `rknn_benchmark`, `librknn_api_android_demo`) plus Toolkit2 `examples/functions` orientation.
- `references/21_custom_op.md` — distilled from Toolkit2 `examples/functions/custom_op/*` and `rknpu2/examples/rknn_custom_op_demo/{cpu,gpu,pytorch}`.
- `references/22_onnx_edit_and_graph_tools.md` — distilled from Toolkit2 `examples/functions/onnx_edit`, `npu_device_test`, `hybrid_quant`, `multi_input`, and the functions directory map.
- `references/23_sparsity_and_toolkit_functions.md` — distilled from SDK `autosparsity/` READMEs, `examples/functions/model_pruning`, and `config(sparse_infer=...)` guidance.
- `references/24_maintainer_update.md` — maintainer-only procedure for refreshing the skill for a new official SDK release. Not an end-user task route.
