# Official Examples and Smoke Paths

Use this file when the user needs the **smallest official validation path** before touching a business demo or custom application. These examples are the fastest way to separate "SDK/runtime chain is broken" from "my model/app logic is broken".

## Why these examples matter

The official examples are narrower and more diagnostic than a full Model Zoo demo:

- `rknn_common_test` — minimal "can this model run at all" validation
- `rknn_benchmark` — performance-oriented smoke path
- `librknn_api_android_demo` — minimal Android HIDL / `librknn_api_android.so` path

Use them before debugging a large custom app when the basic runtime chain is still uncertain.

## 1. `rknn_common_test`

Best for:

- verifying a board can load and run a normal `.rknn`
- checking the base runtime/lib path before porting a full app
- confirming the issue is not in model-specific postprocess

The upstream example supports Linux and Android packaging and runs a known model/image pair:

```bash
./rknn_common_test model/<TARGET_PLATFORM>/mobilenet_v1.rknn model/dog_224x224.jpg
```

V2.3.2 README lists RK3566_RK3568, RK3562, RK3576, RK3588, and RV1126B. Treat it as the smallest official general-runtime smoke test.

When to prefer it:

- user says "模型就是跑不起来"
- user has a custom app but not yet a clean runtime baseline
- you want to isolate runtime init/load issues from app code

## 2. `rknn_benchmark`

Best for:

- establishing a pure runtime performance baseline
- checking loop-count behavior
- checking multi-input invocation syntax
- validating core-mask behavior on supported SoCs

```bash
./rknn_benchmark xxx.rknn [input_data] [loop_count] [core_mask]
```

Important constraints from the upstream example:

- V2.3.2 README lists RK3566_RK3568, RK3562, and RK3588 (not RK3576/RV1126B).
- `core_mask`: `0` auto, `1` core1, `2` core2, `4` core3, `3` core1+2, `7` all. **Only RK3588 supports core mask.**
- benchmark numbers are about the runtime path, not end-to-end app latency
- it accepts image, `.npy`, and multi-input `input1.npy#input2.npy` style inputs
- do not generalize RK3588 core-mask settings to other SoCs

Use this before attributing a performance problem to the model itself.

## Runtime C example map (`rknpu2/examples/`)

Use these when you need a working C/C++ reference instead of writing from scratch:

| Example | Use for |
|---|---|
| `rknn_mobilenet_demo` | smallest general-API baseline |
| `rknn_api_demo` | `rknn_create_mem` zero-copy, RGA + zero-copy, internal-mem-from-fd, MMZ variants |
| `rknn_zero_copy/` | README map of all zero-copy demos on full-runtime and RV110x platforms |
| `rknn_multiple_input_demo` | multi-input general API |
| `rknn_dynamic_shape_input_demo` | dynamic-shape C, including zero-copy variants |
| `rknn_internal_mem_reuse_demo` | `RKNN_FLAG_MEM_ALLOC_OUTSIDE` / internal memory reuse |
| `rknn_matmul_api_demo` | Matmul API, including dynamic-shape variant |
| `rknn_common_test` / `rknn_benchmark` | runtime load/run baseline and pure inference perf |
| `rknn_custom_op_demo/{cpu,gpu,pytorch}` | board-side custom op registration |
| `librknn_api_android_demo` | Android HIDL smoke path |
| `RV1106_RV1103/` | RV110x-specific mobilenet / yolov5 C demos |

Exact directory contents vary slightly by SDK release; treat this as a map, not a guarantee.

## 3. `librknn_api_android_demo`

Best for:

- validating the Android **HIDL** path specifically
- confirming `librknn_api_android.so` and the HAL service stack are wired correctly
- separating "APK camera/demo issue" from "Android NPU API path issue"

This example is **not** the same thing as the YOLOv5 Android APK demo. It is the minimal HIDL-facing Android smoke path and should be referenced whenever the user is using `librknn_api_android.so`.

For a full-runtime Linux board, `examples/rknn_zero_copy/README.md` is the upstream index of the zero-copy variants; use it when the user asks which `rknn_api_demo` source to copy.

Use it when:

- the user asks about CTS/VTS-safe Android integration
- the user mentions `librknn_api_android.so`
- the direct-link `librknnrt.so` path is not allowed

Keep this file focused on **when to use the example**. For the actual Android packaging model, HIDL vs direct-link rules, and runtime library layout, defer to `references/12_android_deploy.md`.

## Suggested triage order

When the user has a custom app problem:

1. Can the same `.rknn` run in `rknn_common_test`?
2. If yes, what does `rknn_benchmark` say about runtime-only throughput?
3. If Android HIDL is involved, can `librknn_api_android_demo` run?
4. Only after these pass, debug business preprocessing, postprocess, JNI glue, UI, or app lifecycle.

## 4. Toolkit2 `examples/functions/*` (capability smoke)

When the base runtime works but a **Toolkit2 feature** is unclear, use the matching function example in the user's SDK (full map: `references/22_onnx_edit_and_graph_tools.md`):

- connected debug all-in-one → `npu_device_test`
- hybrid quant steps → `hybrid_quant`
- multi-input dataset → `multi_input`
- onnx I/O reshape → `onnx_edit`
- custom op convert → `custom_op` (+ board `rknn_custom_op_demo` in `references/21_custom_op.md`)
- sparse prune → `model_pruning` (+ AutoSparsity in `references/23_sparsity_and_toolkit_functions.md`)

## 5. Board custom-op demos

Under the user's `rknpu2/examples/rknn_custom_op_demo/`:

- `rknn_custom_cpu_op_demo` — CPU custom op
- `rknn_custom_gpu_op_demo` — GPU custom op
- `rknn_custom_pytorch_op_demo` — PyTorch-exported custom op

Only after stages A/B in `references/21_custom_op.md` produce a valid custom `.rknn`.

## How this complements existing references

- Use `references/05_runtime_c_api.md` for API rules and platform support tables
- Use `references/06_performance_memory.md` for optimization interpretation
- Use `references/12_android_deploy.md` for Android packaging, HIDL vs direct-link guidance, and runtime library layout
- Use `references/10_model_zoo_workflows.md` for large demo porting and model-family workflows
- Use `references/21_custom_op.md` / `22_onnx_edit_and_graph_tools.md` / `23_sparsity_and_toolkit_functions.md` for advanced Toolkit2 capabilities

## Common mistakes

- Jumping straight into a full Model Zoo or business demo before proving the base runtime path
- Using performance results from a full app to infer NPU runtime speed without a benchmark baseline
- Treating `librknn_api_android_demo` and the YOLOv5 APK demo as interchangeable
- Debugging postprocess when the model cannot even load in a minimal official test
- Opening custom-op demos before Toolkit2 registration / conversion succeeds
