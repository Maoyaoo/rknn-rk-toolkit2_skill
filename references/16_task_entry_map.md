# RKNN Task Entry Map

Use this file when the request is still broad or does not match a single Common Tasks row in `SKILL.md` / `routing.yaml`. Prefer matching a workflow first; this table is the expanded map.

## How to use this map

1. Prefer `routing.yaml` / `SKILL.md` Common Tasks → open the matching `workflows/*.md`.
2. If still ambiguous, identify the task shape from the table below.
3. Read Always Read (`conventions/agent-response.md`, `references/01_platform_matrix.md`) then the listed distilled references.
4. Use the listed scripts/assets/templates to build the concrete patch or command sequence.
5. If a key detail is still missing, escalate with `references/15_rknn_doc_lookup.md` / `workflows/doc-lookup.md`.
6. Before answering, shape the response with `references/17_response_contracts.md`.
7. Right before sending, run `references/18_review_checklist.md`.

## Task routing table

| User request pattern | Workflow | Read first | Then use | Escalate to raw docs when |
|---|---|---|---|---|
| Toolkit2 install / host env / `from rknn.api import RKNN` fails | `workflows/convert-model.md` | `references/02_toolkit2_conversion.md` (host environment) | `scripts/rknn_env_probe.py`; `08_wsl.md` for WSL | exact package/wheel or OS-support question |
| Convert ONNX/PyTorch model to `.rknn` | `workflows/convert-model.md` | `references/02_toolkit2_conversion.md` | `assets/convert_onnx_to_rknn.py`, `assets/convert_pytorch_to_rknn.py`, `scripts/make_dataset_txt.py` | exact `config()` knob, framework edge case, or op constraint unclear |
| Quantization accuracy is poor / output is wrong | `workflows/quant-accuracy.md` | `references/03_quantization_accuracy.md`, `02_toolkit2_conversion.md` | `assets/accuracy_analysis_template.py`, `scripts/make_dataset_txt.py` | exact quant/API rules or build error cause needed |
| `rknn_server` / connected inference / board init fails | `workflows/board-debug.md` | `04_board_debug_rknn_server.md`, `09_troubleshooting.md`, `gotchas/high-cost.md` | `scripts/rknn_env_probe.py`, `scripts/rknn_server_commands.py` | server/runtime error text or transport rule still ambiguous |
| Need C/C++ inference code on board | `workflows/runtime-c.md` | `references/05_runtime_c_api.md` | general/multi-input C templates + CMake | exact RKNNRT API or platform support matrix unclear |
| Need RV110x / RK2118 zero-copy style code | `workflows/runtime-c.md` + `workflows/platform-special.md` | `05_runtime_c_api.md`, `14_rk2118_rtos_deploy.md`, `06_performance_memory.md` | zero-copy / RGA templates | mini-runtime restriction or SoC behavior unclear |
| Need dynamic shape / external memory / shared memory | `workflows/runtime-c.md` | `11_dynamic_shape_and_mem_reuse.md`, `05_runtime_c_api.md` | dynamic-shape + internal-mem templates | exact query/flag/platform support unclear |
| Need performance / memory optimization | `workflows/platform-special.md` | `06_performance_memory.md`, optionally `13_advanced_features.md` | env probe, RGA zero-copy template | exact SoC capability or op fallback unclear |
| Model encryption / multi-batch / weight sharing / NPU SRAM | `workflows/platform-special.md` | `13_advanced_features.md`, `05_runtime_c_api.md` | advanced templates + init flags | exact flag/API or supported-platform list unclear |
| Need Model Zoo porting / demo adaptation | `workflows/model-zoo-port.md` | `10_model_zoo_workflows.md`, `09_troubleshooting.md` | closest `assets/c_api_*`, `lite2_python_infer.py` | model-specific op/export constraint unclear |
| Need board-side Python inference on an existing `.rknn` | `workflows/platform-special.md` | `19_toolkit_lite2_board_python.md` | `assets/lite2_python_infer.py` | Lite2 runtime or dynamic-shape support unclear |
| Need Android packaging / JNI deployment | `workflows/platform-special.md` | `12_android_deploy.md`, `05_runtime_c_api.md` | CMake template + matching C API templates | Android HIDL/packaging rule unclear |
| Need RK2118 RT-Thread deployment | `workflows/platform-special.md` | `14_rk2118_rtos_deploy.md`, `05_runtime_c_api.md` | zero-copy skeleton | RK2118 quick-start or mini-runtime detail unclear |
| Need operator support answer | `workflows/operator-support.md` | `07_operator_support.md`, `15_rknn_doc_lookup.md` | no template first | attrs/dtype/rank/SoC → raw op list |
| Custom op / `reg_custom_op` / board custom CPU/GPU op | `workflows/graph-advanced.md` | `21_custom_op.md`, `05_runtime_c_api.md` | user's Toolkit2 `custom_op` + `rknn_custom_op_demo` | C callback / platform matrix in raw RKNNRT ref |
| `onnx_edit` / I/O reshape for NPU | `workflows/graph-advanced.md` | `22_onnx_edit_and_graph_tools.md` | Toolkit2 `examples/functions/onnx_edit` | equation edge cases in User Guide |
| AutoSparsity / `sparse_infer` / `model_pruning` | `workflows/graph-advanced.md` | `23_sparsity_and_toolkit_functions.md` | SDK `autosparsity` + `model_pruning` example | SoC sparse column in compiler list |
| Need WSL / adb-on-WSL guidance | `workflows/platform-special.md` | `08_wsl.md`, `04_board_debug_rknn_server.md` | `scripts/rknn_env_probe.py` | exact transport/tooling behavior unclear |
| Need the smallest official runtime smoke test before debugging a full app | `workflows/platform-special.md` | `20_official_examples_and_smoke_paths.md`, `05_runtime_c_api.md` | official common_test / benchmark / Android demo path | minimal official example still does not explain the failure |
| Exact API / numeric limit / raw error line | `workflows/doc-lookup.md` | `15_rknn_doc_lookup.md`, `00_source_map.md` | `rg` into `rknn_doc/` | after raw docs still missing → user SDK headers/PDFs |

## Fast decision hints

- If the user mentions install / `pip install rknn-toolkit2` / `from rknn.api import RKNN` / Python version, start from the host-environment section of `references/02_toolkit2_conversion.md`.
- If the user mentions `target_platform`, `load_onnx`, `build()`, `dataset.txt`, start from conversion.
- If they mention `RKNN_ERR_*`, `abnormal server`, `adb devices`, `device unavailable`, start from troubleshooting + board debug.
- If they mention `rknn_inputs_set`, `rknn_set_io_mem`, `librknnmrt.so`, or `zero-copy`, start from runtime C API.
- If they mention `RK2118`, `RV1106`, `RV1103`, `serial`, `RT-Thread`, or `SYSTEM DEBUG`, branch to the platform-specific path immediately.
- If they mention `MAP`, `cosine`, `accuracy_analysis`, `hybrid_quantization`, start from quantization accuracy.
- If they mention `NonMaxSuppression`, `Softmax`, `ConvTranspose`, `unsupported op`, or `fallback to CPU`, start from operator support, then escalate to raw docs if attrs matter.
- If they mention `reg_custom_op`, `cst*`, `onnx_edit`, `sparse_infer`, `AutoSparsity`, or `model_pruning`, start from `workflows/graph-advanced.md`.
- If they mention `export_encrypted_rknn_model`, `rknn_batch_size`, `RKNN_FLAG_SHARE_WEIGHT_MEM`, or `RKNN_FLAG_ENABLE_SRAM`, start from `workflows/platform-special.md` → `references/13_advanced_features.md`.
- If they mention `rknn_convert` / a `.yml` model config, start from `workflows/convert-model.md` → `references/02_toolkit2_conversion.md`.

## Minimal working sets

When the request is still ambiguous, these are the smallest safe bundles:

- Conversion task:
  `references/02_toolkit2_conversion.md` + `references/01_platform_matrix.md` + one conversion asset
- Runtime C task:
  `references/05_runtime_c_api.md` + `references/01_platform_matrix.md` + one C template
- Troubleshooting task:
  `references/09_troubleshooting.md` + `references/04_board_debug_rknn_server.md` + `scripts/rknn_env_probe.py`
- Raw-doc escalation:
  `references/15_rknn_doc_lookup.md` + matching `rknn_doc/*.md`

## Common routing mistakes

- Jumping into `rknn_doc/` before reading the distilled references.
- Giving a generic C template to RV110x/RK2118 without checking the general-API vs mini-runtime split.
- Reusing a Model Zoo postprocess path without reading the corresponding conversion notes first.
- Treating all `device unavailable` failures as the same issue without separating adb / `rknn_server` / runtime-version mismatch / wrong target SoC.
