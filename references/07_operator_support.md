# Operator Support Checks

Never claim an op is supported just because ONNX Runtime, PyTorch, or TensorFlow supports it. RKNN support depends on source framework, opset, target SoC, tensor rank/layout/dtype, quantization mode, attributes, and whether the compiler maps it to NPU, CPU fallback, custom op, or rejects it.

## Check order

1. Inspect the installed SDK docs and `rknn_api.h`/Toolkit2 version.
2. Check `doc/RKNNToolKit2_OP_Support-*.md` for quick framework notes.
3. Check `05_RKNN_Compiler_Support_Operator_List_*.pdf` for exact constraints.
4. Build with verbose logs and inspect compiler warnings, CPU fallback, or rejected ops.
5. If using Model Zoo, compare with the closest export guide because many demos patch model export for RKNN.

## Common unsupported or constrained ONNX areas

Examples in V2.3.2 support notes include unsupported or constrained `NonMaxSuppression`, `GridSample`, `Loop`, `Scan`, `Einsum`, `DFT`, `STFT`, `Range`, `NonZero`, random/window/optional/sequence/string ops, constrained `Resize`, constrained `Softmax`/`Slice`/`LogSoftmax`, and recurrent/ROI restrictions.

This is only a fast screen. The full compiler support list is authoritative for the SDK release.

## Cross-cutting constraints (V2.3.2 compiler list)

These numeric limits recur across many operators and explain otherwise-mysterious build failures. Numbers come from the V2.3.2 compiler support document bundled in this skill as `rknn_doc/MinerU_markdown_05_RKNN_Compiler_Support_Operator_List_*.md`. Always re-check the SDK release in use — these are V2.3.2 values.

### Tensor rank and dtype

- Nearly all NPU ops cap input tensor rank at **4** (`[batch, channel, height, width]`). Higher-rank tensors must be reshaped before the op and restored after.
- Most NPU ops accept only **INT8 or FLOAT16**. FLOAT32 is CPU-fallback territory — a model that "compiles" but runs slowly often has float32 ops silently falling back to CPU. Inspect verbose build logs for CPU-fallback warnings.

### First-layer input width × dilation (Compiler Op List §5.1)

The first-layer input width is bounded by both an absolute cap AND a width × dilated_kernel_h product cap. The product cap depends on SoC, first-layer precision, and the first conv-class op type (Conv / DepthwiseConv / ConvTranspose). All platforms use the form:

```
width × dilated_kernel_h ≤  C × N
width                       ≤  Wmax
N must be an integer in [1, 7]
```

Int8 NPU-path values (the common deployment case):

| Platform | C | Wmax | N for Conv / ConvTranspose | N for DepthwiseConv |
|---|---|---|---|---|
| **RK3566 / RK3568** | 1024 | 4096 | `8 − CEIL(dk_h × dk_w / 128)` | `8 − CEIL(dk_h × dk_w / 4096)` |
| **RK3588** | 2048 | 8192 | `12 − MAX(CEIL(dk_h × dk_w / 128), 3)` | `12 − MAX(CEIL(dk_h × dk_w / 2048), 3)` |
| **RK3562 / RV1103 / RV1106** | 2048 | 4096 | `8 − MAX(CEIL(dk_h × dk_w / 128), 2)` | `8 − CEIL(dk_h × dk_w / 4096)` |
| **RK3576 / RV1126B** | 2048 | 4096 | `8 − MAX(CEIL(dk_h × dk_w / 128), 2)` | `8 − CEIL(dk_h × dk_w / 2048)` |
| **RV1103B / RV1106B** | 2048 | 4096 | `8 − MAX(CEIL(dk_h × dk_w / 128), 2)` | `8 − CEIL(dk_h × dk_w / 4096)` |

`dk_h = dilated_kernel_h`, `dk_w = dilated_kernel_w` — see the dilation formula above.

**Float16 first layers** use different constants/formulas on some SoCs. The important ones:

| Platform | C | Wmax | Conv / ConvTranspose N | Depthwise N |
|---|---|---|---|---|
| RK3566 / RK3568 | 1024 | 4096 | `8 − CEIL(D / 128)` | `8 − CEIL(D / 4096)` |
| RK3588 | **1024** (not 2048) | 8192 | `12 − MAX(CEIL(D / 128), 3)` | `12 − MAX(CEIL(D / 2048), 3)` |
| RK3562 | 2048 | 4096 | `8 − MAX(CEIL(D / 128), 2)` | `8 − CEIL(D / 4096)` |
| RK3576 / RV1126B | 2048 | 4096 | `8 − MAX(CEIL(D / 128), 2)`; ConvTranspose `12 − MAX(CEIL(D / 128), 2)` | `8 − MAX(CEIL(D / 2048), 3)` |
| RK2118 (FP16 only) | 2048 | 4096 | `8 − MAX(CEIL(D / 128), 2)` | `8 − CEIL(D / 4096)` |

`D = dk_h × dk_w`. RK3566/RK3568 float16 keeps the same N formulas as int8 but halves the width alignment to 4 (see `references/05_runtime_c_api.md`). Read §5.1 of `rknn_doc/MinerU_markdown_05_*.md` before asserting a cap not listed here.

If your model fails to compile with a width-related error on the first conv, drop input width or kernel size, or eliminate dilation. Only first-layer width is bounded this way; later layers can use larger widths.

### Convolution family

- `Conv` / `ConvTranspose`: `kernel_h, kernel_w ∈ [1, 31]`.
- `DepthwiseConv`: `kernel_h, kernel_w ∈ [1, 8]`.
- `Conv` strides: `stride_h, stride_w ∈ [1, 7]`.
- `Conv` pads: each of `top/left/bottom/right ∈ [0, 15]`.
- `Conv` dilations: `dilations_h, dilations_w ∈ [1, 31]`.
- **Dilation formula** (Compiler Op List §6): `dilated_kernel_h = kernel_h × dilations_h − dilations_h + 1` (same for w). Use this when computing width limits — most "dilation" caps in this doc refer to `dilated_kernel_h`, not raw `dilations_h`.
- When `dilated_kernel_h > 1`, input `width < 16383`. Same cap appears on the first-layer input width — see `references/05_runtime_c_api.md` for the full first-layer width × dilation table.

### Softmax / Hardmax

- Only `axis=1` and `axis=3` / `axis=-1` are supported.
- `axis=1`: requires `height * width < 16384`.
- `axis=3` or `axis=-1`: requires `channel * height < 16384` and is also bound by `Transpose` limits.
- This is the most common reason transformer-style heads fail to compile.

### Recurrent ops (RNN / LSTM / GRU)

- `batchsize < 8192` (the input tensor bound is `(0, 8192)`), and when `batchsize > 1` it **must be a multiple of 4**.
- `seqlength`, `hiddensize`, input/hidden dims all `< 8192`.
- `num_directions` is 1 or 2.

### Slice

- NPU path only supports `steps != [1,1,1,1]` when `height * width == 1`, `channel % subc == 0`, `starts[1] % subc == 0`, `ends[1] % subc == 0`, and `steps[1] <= subc`. Otherwise `steps` must be all ones or the op falls back to CPU.

### Platform-gated ops

Newer fused ops (RMSNorm, FlashAttention, exSDP, certain transformer fusions) ship only on **RK3588 / RK3576** in V2.3.2. RV1103/RV1106/RK2118 and RK356x lack them — porting a transformer demo from RK3588 to a smaller SoC usually requires graph changes, not just a re-build.

## Operator fusion rules (User Guide §8.4.2)

RKNN's compiler fuses specific Conv+activation/add patterns. Designing the model so these patterns are present (or rewriting the graph to expose them) improves both perf and accuracy. V2.3.2 fusion rules:

**Already supported:**

- Conv + Relu
- Conv + PRelu / LeakyRelu
- Conv + Clip
- Conv + Sigmoid / Tanh / Elu / Silu
- Conv + Add
- Conv + Activation + Add

**Planned for later releases** (treat as unsupported today):

- Activation + Add / Mul
- Add / Mul + Activation
- Conv + Mul
- Conv + Activation + Mul
- Conv + Activation + Pooling
- Conv + Activation + Add / Mul + Pooling

### Subgraph → single-op fusion (User Guide §8.5.3)

Two transformer-critical subgraph patterns are recognized and replaced with a single fused op:

- **GLU** ← `Split + Sigmoid + Mul`
- **LayerNorm** ← `ReduceMean + Sub + Pow + ReduceMean + Add + Sqrt + Div` (optionally followed by `+ Mul + Add`)

If a ported transformer's GLU or LayerNorm runs as multiple separate ops, the subgraph did not match — re-export the model so the exact op sequence appears contiguously in the graph.

## Graph-level rewrite patterns (User Guide §8.4)

When the compiler does not fuse or simplify the way you expect, these manual rewrites have proven useful:

- **Channel shuffle → Conv**: replace `Transpose + Reshape` shuffles with a 0/1-weighted `Conv` that just permutes channels. Used in ShuffleNetV2 ports.
- **Swap Clip and Transpose order** so that `Conv + Clip` becomes fusable.
- **Slice → Conv**: YOLOv5-nano folds multiple `Slice` ops into one `Conv` to simplify the head.
- **Merge consecutive Reshape / Transpose / Slice / Add / Mul / Sub / Div** — combine into a single op.
- **Move NMS / decode / top-k to CPU postprocess** — NonMaxSuppression is unsupported; clip the graph at `rknn.load_onnx(outputs=[...])` and run NMS on CPU.

## Fast screen before opening the PDF

If a model fails to compile or runs slowly, check in this order:

1. Any tensor rank > 4 anywhere? Reshape to 4D.
2. Any op needing FLOAT32 (e.g. certain reductions, complex math)? Expect CPU fallback.
3. Conv with kernel > 31, or depthwise with kernel > 8? Won't compile.
4. Dilated conv with large width? Width must be < 16383.
5. Softmax over a large axis? Check the 16384 product caps above.
6. RNN/LSTM/GRU with batch not a multiple of 4? Pad to a multiple of 4.
7. Transformer-style ops on a non-RK3588/3576 target? Likely unsupported fused op.

## Rewrite strategies

- Move NMS/decode/top-k/mask generation/postprocess to CPU code.
- Replace unsupported activations/normalization with equivalent supported subgraphs.
- Freeze dynamic constants and shapes before export.
- Simplify tensors above 4D when possible.
- Use `op_target` for selected CPU fallback only when the API and platform support it.
- Use custom ops only when unavoidable and supported by the target runtime — full path in `references/21_custom_op.md` / `workflows/graph-advanced.md`.
- Try I/O reshape via `onnx_edit` (`references/22_onnx_edit_and_graph_tools.md`) when rank/layout blocks NPU fusion before inventing a custom kernel.

## Model Zoo export caution

YOLO demos often require RKNN-specific export branches or graph changes. If a custom model keeps the original graph, postprocess may need to change and accuracy/performance may drop.
