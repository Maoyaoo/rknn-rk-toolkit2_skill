# Custom Operators (Toolkit2 + Runtime)

Last-resort path when RKNN cannot compile an op as NPU, graph rewrite is impossible, and CPU postprocess is not acceptable. Prefer rewrite / host postprocess first — custom ops usually run on CPU (or OpenCL GPU for selected demos) and are slower than supported NPU ops.

Distilled from Toolkit2 `examples/functions/custom_op/*`, Runtime `rknpu2/examples/rknn_custom_op_demo/*`, and `rknn_custom_op.h` notes. Package paths below are **inside the user's RKNN-Toolkit2 / rknpu2 install**, not this skill package.

## When custom op is justified

1. A single unsupported op blocks conversion after checking `references/07_operator_support.md` + `rknn_doc` op lists.
2. A long chain of tiny ops is faster as one fused custom kernel (NEON/OpenCL) than many NPU↔CPU round-trips.
3. Postprocess must live in-graph for product reasons (rare).

Do **not** use custom ops for:

- RV1103/RV1106/B and RK2118 unless the installed SDK docs explicitly list support (matrix in `references/05_runtime_c_api.md` marks custom-op APIs unavailable on mini-runtime family).
- Android **HIDL** path (`librknn_api_android.so`) — custom op / matmul often stripped; use direct-link `librknnrt.so` (`references/12_android_deploy.md`).

## Three stages (always keep them separate)

| Stage | Where | Goal |
|---|---|---|
| A. Produce ONNX containing a non-standard op | PyTorch export / ONNX graph edit | Graph carries `cst*` (or similar) nodes |
| B. Convert ONNX → RKNN with Toolkit2 registration | PC Toolkit2 | `rknn.reg_custom_op(...)` before `load_onnx` / `build` |
| C. Run on board with Runtime registration | C/C++ Runtime demo | `rknn_register_custom_ops` (+ CPU or GPU implementation) |

Simulator-only conversion can stop after B with a `compute()` implementation for PC verification. Board deployment needs C.

## Stage A — get a custom-op ONNX

### Path A1: Replace an existing ONNX op (non-ONNX-standard name)

Toolkit2 example family: `examples/functions/custom_op/non-onnx_standard/`.

1. Edit ONNX (or script) so the target node type becomes a custom name, e.g. `Sigmoid` → `cstSigmoid`.
2. Keep tensor shapes/dtypes unchanged unless you also implement matching shape inference.
3. Downstream conversion must register the same `op_type` string.

### Path A2: Export from PyTorch when ONNX mapping is missing or you want fusion

Toolkit2 example family: `examples/functions/custom_op/gen_custom_onnx_from_pytorch/`.

| Situation | Mechanism | Sub-example |
|---|---|---|
| PyTorch op has no ONNX mapping (e.g. `torch.nn.Threshold`) | Register ONNX symbolic only (`torch.onnx.register_custom_op_symbolic`) | `register_onnx_symbolic/` |
| Fuse many tiny jit ops into one onnx op for perf | Register custom JIT op **and** onnx symbolic | `register_pytorch_op/` |

`torch.onnx.export` first traces to JIT, then maps JIT ops to ONNX. Choose the branch that matches which layer is missing.

### Path A3: ONNX already has custom nodes

Toolkit2 example: `examples/functions/custom_op/convert_custom_onnx_to_rknn/` (`cstDualResidual` multi-I/O).

Skip graph rewrite; go to Stage B.

## Stage B — Toolkit2 registration and conversion

**Toolkit2 constraint (API Ref):** `reg_custom_op` currently supports **ONNX models only**. PyTorch custom paths still export ONNX first, then register on the ONNX graph.

Register **before** `load_onnx` / `build`:

```python
from rknn.api import RKNN
from rknn.api.custom_op import get_node_attr  # if attrs needed

class cstDualResidual:
    op_type = 'cstDualResidual'  # must match ONNX node op_type

    def shape_infer(self, node, in_shapes, in_dtypes):
        # return list of out shapes, list of out dtypes
        return in_shapes.copy(), in_dtypes.copy()

    def compute(self, node, inputs):
        # PC simulator / accuracy path — numpy
        x, y = inputs[0], inputs[1]
        alpha = get_node_attr(node, 'alpha')
        return [x * alpha - y, y * alpha - x]

rknn = RKNN(verbose=True)
rknn.config(target_platform='rk3588')
ret = rknn.reg_custom_op(cstDualResidual())
ret = rknn.load_onnx(model='model_with_custom.onnx')
ret = rknn.build(do_quantization=False)  # often start FP to validate shapes
ret = rknn.export_rknn('model_custom.rknn')
# optional: rknn.init_runtime(); rknn.inference(...); compare golden
rknn.release()
```

Rules:

- `op_type` string is the contract between ONNX, Toolkit2, and Runtime.
- `shape_infer` must be correct — wrong shapes fail later at Runtime more painfully than at convert time.
- `compute` is for **simulator / golden checks** on the host. Board C kernels are separate.
- Quantize only after FP custom-op path matches golden; custom kernels may not quantize the same way as NPU ops.
- Multi-input models: same `dataset.txt` / `inference(..., data_format=[...])` rules as normal multi-input (`references/02_toolkit2_conversion.md`).

## Stage C — board Runtime (C API)

Official demos under user's `rknpu2/examples/rknn_custom_op_demo/` (prebuilt custom `.rknn` samples exist for RK3562 / RK3566_RK3568 / RK3576 / RK3588 / RV1126B on several demos — still **not** mini-runtime RV110x/RK2118):

| Demo | Backend | Typical op example | Notes |
|---|---|---|---|
| `rknn_custom_cpu_op_demo` | CPU | `cstSigmoid` (YOLOX) | Pair with Toolkit2 non-onnx_standard conversion |
| `rknn_custom_gpu_op_demo` | GPU/OpenCL | custom `ArgMax` (PP-HumanSeg) | Needs OpenCL fields on `rknn_custom_op`; no RV1126B sample in some trees — check user's package |
| `rknn_custom_pytorch_op_demo` | CPU | dual residual from PyTorch export | Conversion scripts under demo `convert_rknn_demo/` |

### Registration order (from `rknn_custom_op.h`)

1. `rknn_init` as usual.
2. Fill `rknn_custom_op` array: `version`, `target` (`RKNN_TARGET_TYPE_CPU` or `_GPU`), `op_type` string matching the graph.
3. Set callbacks: **`compute` required**; `init` / `prepare` / `destroy` optional; `compute_native` not supported in current headers — leave NULL.
4. GPU only: set `cl_kernel_name`, `cl_kernel_source` / size, `cl_build_options`.
5. `rknn_register_custom_ops(ctx, ops, n_ops)` **after** `rknn_init`.
6. Then query / set IO / run as normal.

Minimal CPU skeleton (mirror demo, not a full product kernel):

```c
#include "rknn_api.h"
#include "rknn_custom_op.h"

static int compute_cst(rknn_custom_op_context* op_ctx,
                       rknn_custom_op_tensor* inputs, uint32_t n_inputs,
                       rknn_custom_op_tensor* outputs, uint32_t n_outputs) {
  (void)op_ctx; (void)n_inputs; (void)n_outputs;
  /* read inputs[i].mem.virt_addr + inputs[i].attr; write outputs[j] */
  return 0;
}

/* after rknn_init(&ctx, ...) */
rknn_custom_op user_op;
memset(&user_op, 0, sizeof(user_op));
strncpy(user_op.op_type, "cstSigmoid", RKNN_MAX_NAME_LEN - 1);
user_op.version = 1;
user_op.target  = RKNN_TARGET_TYPE_CPU;
user_op.compute = compute_cst;
int ret = rknn_register_custom_ops(ctx, &user_op, 1);
```

Helpers:

- `rknn_custom_op_get_op_attr(op_ctx, attr_name, &attr)` for node attributes inside callbacks.
- If `init` returns `RKNN_WARNING_SKIP_CUSTOM_OP_COMPUTE` (**-14**) and RKNN already supports that op type, Runtime may fall back to the built-in implementation.

### C field quick-ref (V2.3.2 `rknn_custom_op.h` / RKNNRT API Ref)

Authoritative details stay in the installed header. This table is only to stop inventing field names.

| Type | Fields agents must get right |
|---|---|
| `rknn_target_type` | `RKNN_TARGET_TYPE_CPU = 1`, `RKNN_TARGET_TYPE_GPU = 2` |
| `rknn_custom_op_context` | `target`, `internal_ctx`, `gpu_ctx` (OpenCL: `cl_context` / `cl_command_queue` / `cl_kernel`), `priv_data` (user-owned) |
| `rknn_custom_op_tensor` | `attr` (`rknn_tensor_attr`) + `mem` (`rknn_tensor_mem`) — use `mem.virt_addr` / `fd` / `size` with `attr` for shape/dtype |
| `rknn_custom_op_attr` | `name`, `dtype`, `n_elems`, `data` — cast `data` using `dtype` after `rknn_custom_op_get_op_attr` |
| `rknn_custom_op` | `version`, `target`, `op_type[]`; GPU: `cl_kernel_name`, `cl_kernel_source`, `cl_source_size` (**0 = filepath**, **>0 = string length**), `cl_build_options`; callbacks `init?`, `prepare?` (once per `rknn_run`), **`compute` required**, `compute_native` **unsupported → NULL**, `destroy?` |
| Register API | `rknn_register_custom_ops(ctx, op_array, n)` — `ctx` **already** `rknn_init`'d |
| Platform matrix | `rknn_register_custom_ops` / `rknn_custom_op_get_op_attr`: **yes** on RV1126B/RK356x/RK3588/RK3576 class; **no** on RV110x / RK2118 (RKNNRT API Ref support table) |

Callback timing (API Ref):

- `init` — once at registration  
- `prepare` — once per `rknn_run` (optional)  
- `compute` — once per `rknn_run` (required)  
- `destroy` — once at `rknn_destroy`

### Connected accuracy analysis with custom ops (User Guide §5.5.4.4)

Running `accuracy_analysis(target=<SoC>)` on a custom-op model goes through a **plugin** mechanism, not the in-process C registration above:

- Requires `rknn_server` >= **1.6.0**.
- The board-side server `dlopen`s the plugin library, so the plugin must export `get_rknn_custom_op` and a library name prefixed `librkcst_` (for example `librkcst_relu.so`).
- Plugin directories: `/vendor/lib64/` on Android arm64-v8a; `/usr/lib/rknpu/op_plugins` on Linux.
- Build the plugin against the same Runtime headers as the board; a mismatched `rknn_custom_op` layout fails at `dlopen`/op lookup.

This is separate from the board-app path: an app still calls `rknn_register_custom_ops` after `rknn_init`.

Checklist:

1. Include `rknn_custom_op.h` only when using this path; link a Runtime that exports custom-op symbols (not HIDL-stripped android API).
2. Register **after** `rknn_init`, with `op_type` identical to Toolkit2 / ONNX.
3. Ship the `.rknn` built with matching Toolkit2 `reg_custom_op` — version skew fails op lookup.
4. Platforms: full Runtime family. Mini-runtime: refuse.

API matrix: `references/05_runtime_c_api.md`.

## Decision tree

```text
Unsupported op?
  ├─ Can rewrite graph / move to host postprocess? → do that
  ├─ Can onnx_edit reshape/transpose fix layout so NPU supports path?
  │     → references/22_onnx_edit_and_graph_tools.md
  ├─ Need custom op
  │     ├─ Graph rewrite of existing ONNX op name → Stage A1 + B + C(cpu/gpu demo)
  │     ├─ PyTorch export issue / fusion → Stage A2 + B + C
  │     └─ Already custom ONNX → Stage B + C
  └─ Mini-runtime SoC? → refuse custom-op path; redesign graph
```

## Pitfalls

- Calling `reg_custom_op` on non-ONNX loads — Toolkit2 documents ONNX-only registration.
- Registering after `load_onnx` — too late; load/build never sees the op.
- Mismatched `op_type` strings between ONNX edit, Python class, and C registration.
- Board: registering custom ops **before** `rknn_init` or forgetting `compute` callback.
- Expecting NPU speed from CPU custom ops.
- Using Model Zoo postprocess assumptions after replacing a middle-graph op (output tensors may change meaning).
- Copying GPU custom demo to a BSP without OpenCL/GPU custom support.
- Expecting connected `accuracy_analysis` to find a custom op without the `librkcst_*` plugin and `rknn_server` >= 1.6.0.
- Using example paths from another SDK version — always describe paths relative to **the user's** SDK install.

## Escalation

- Exact C struct / callback fields → `rknn_doc/MinerU_markdown_04_*.md` + headers in installed Runtime.
- Op still unsupported after custom attempt → compiler list + User Guide custom-op chapters via `workflows/doc-lookup.md`.
