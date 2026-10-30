# ONNX Edit, Graph Tools, and Toolkit2 Function Examples

Use this file when conversion succeeds but the graph is awkward for NPU (extra transpose/reshape, rank issues), or when you need the **official Toolkit2 function-example map** without opening the full SDK tree. Paths named below are relative to the user's **RKNN-Toolkit2 install** (`rknn-toolkit2/examples/functions/...`), not this skill package.

## `rknn.utils.onnx_edit` (User Guide §5.13 / Toolkit2 examples)

Built-in utility to rewrite ONNX **inputs/outputs** so Toolkit2 graph optimization produces a leaner, faster RKNN model.

```python
from rknn.utils import onnx_edit  # import path as in installed Toolkit2

onnx_edit(
    model='in.onnx',
    export_path='out.onnx',
    inputs_transform={},   # name -> equation string
    outputs_transform={},
    dataset=None,          # optional dataset.txt; if set, rewritten dataset is written next to export_path
)
```

### What it does today (V2.3.2 examples)

- Reshape / transpose style adjustments on **named inputs and outputs**.
- Does **not** replace arbitrary middle ops (that is custom-op or manual graph edit territory).
- Dynamic-shape transforms are version-gated: the V2.3.2 README opens by saying dynamic-shape modification is not supported, then later lists split/assignment rules. Verify on the installed Toolkit2 before promising a dynamic-shape `onnx_edit`.

### Transform equation rules (must follow)

Equations look like: `left_symbolic -> right_symbolic`.

1. Separator is `->`. Left = original symbolic shape; right = transformed shape.
2. Symbolic tokens: only **`[a-z]`**, **`,`**, and **`1`**. Other numeric literals are not allowed as free tokens (except `1`) because digits alone cannot disambiguate transpose context.
3. Letters are independent labels — `a,b->b,a` is the same transform idea as `c,a->a,c`.
4. Left side comma-separated rank must match the tensor's rank. Example tensor `[32,4,1,64]` can use `a,b,c,d` or `a,b,1,d`.
5. Every non-`1` character on the left must appear on the right (no dropping real dims).
6. Right side may insert any number of `1` for rank expand, e.g. `a,b,c->a,1,cb,1,1`.
7. Split dims with multi-letter groups + assignment formula in `[ ]`:
   - Example: tensor `[32,4,1,64]` and `ab,c,d,qk[a=2,k=8]->aq,cd,1,kb`
   - One character in a split may be omitted and is inferred; non-integer inference errors out.
8. Dynamic shape (only if the installed Toolkit2 accepts it — see version caveat above):
   - When splitting a dynamic dim, **at most one** of the split pieces may remain dynamic.
   - Assigning values to dynamic dims freezes them to fixed sizes.

### Practical examples

| Goal | Equation sketch |
|---|---|
| 3D → 4D | `a,b,c->a,b,1,c` |
| 5D → 4D | `a,b,c,d,e->ab,c,d,e` |
| Transpose NCHW↔NHWC style | `a,b,c,d->a,d,b,c` |
| Merge + transpose | `a,b,c,d->ad,c,b` |
| Split + merge | `a,bc,de,f[b=2,d=4]->ab,fe,dc,1` |

After `onnx_edit`, logs show inserted reshape/transpose bridge ops between the **new external shape** and the original internal consumer shape. Convert the **edited** ONNX; when you pass `dataset=`, the input transforms are applied to the dataset too and a rewritten copy is written next to `export_path`.

### When to use onnx_edit vs other tools

| Problem | Prefer |
|---|---|
| External layout/rank hurts NPU fusion | `onnx_edit` |
| Single unsupported middle op | graph rewrite, CPU postprocess, or `references/21_custom_op.md` |
| Need C++ starter after a good RKNN | `rknn.codegen()` (`references/02_toolkit2_conversion.md`) |
| Need training-time sparsity | `references/23_sparsity_and_toolkit_functions.md` |

## Official Toolkit2 `examples/functions/` map

Use as the smallest scripted path for a capability (mirror names in the user's SDK):

| Example dir | Skill entry | Use for |
|---|---|---|
| `accuracy_analysis/` | `03_quantization_accuracy.md`, `assets/accuracy_analysis_template.py` | per-layer golden vs quant |
| `hybrid_quant/` | `03_quantization_accuracy.md` | step1 cfg edit → step2 |
| `quantize_algorithm_mmse/` | `03` + `02` | MMSE algorithm switch |
| `dynamic_shape/` | `11_dynamic_shape_and_mem_reuse.md` | Toolkit2 dynamic build |
| `multi_batch/` | `13_advanced_features.md` | `rknn_batch_size` |
| `multi_input/` | `02_toolkit2_conversion.md` | multi-file `dataset.txt` + `data_format` |
| `model_pruning/` | `02` config `model_pruning` + `23` | sparse-weight size cut |
| `onnx_edit/` | this file | I/O reshape/transpose |
| `codegen/` | `02` `codegen()` | auto C++ scaffold |
| `custom_op/` | `21_custom_op.md` | reg_custom_op paths |
| `npu_device_test/` | `04_board_debug_rknn_server.md` | connected list_devices, perf, mem, encrypt, accuracy_analysis on board |

### `npu_device_test` — connected all-in-one checklist

When adb + `rknn_server` work, this example exercises:

- `list_devices()` (ADB mode listing)
- SDK / server / runtime version print
- encrypted export (`export_encrypted_rknn_model`)
- `eval_perf` / layer timing (`perf_debug`)
- memory profile
- TOP-k inference
- `accuracy_analysis` with `target=<SoC>` (simulator + runtime error columns)

Prefer this before inventing a private debug harness for connected mode. Note: `perf_debug` layer timing is **not** available on RV1103/RV1103B/RV1106/RV1106B connected paths (`references/04_board_debug_rknn_server.md`).

### Hybrid quant example operational order

From `hybrid_quant/`:

1. `python step1.py` → produces `*.quantization.cfg` suggestions  
2. **Manually edit** the cfg (proposal is not ground truth)  
3. `python step2.py` → build hybrid model + optional accuracy dump  

Skill API summary remains in `references/03_quantization_accuracy.md`.

### Multi-input calibration reminder

One `dataset.txt` line = space-separated files **in input order**:

```text
dog_128x128.jpg input2.npy input3.npy dog_128x128_gray.png
```

Inference:

```python
rknn.inference(
    inputs=[img, input2, input3, img_gray],
    data_format=['nhwc', 'nchw', 'nchw', 'nhwc'],
)
```

Helper: `scripts/make_dataset_txt.py --multi-input`.

## Pitfalls

- Editing ONNX with Netron-only clicks then forgetting to re-export a valid opset/graph for Toolkit2.
- onnx_edit equation that drops a letter dim or uses numeric tokens other than `1`.
- Applying onnx_edit for dynamic shape without assignment constraints → split failure; the upstream README contradicts itself on dynamic-shape support, so verify on the installed Toolkit2 first.
- Using `npu_device_test` perf numbers as production SLA (example itself warns debug perf can be worse than optimized deploy).
- Using example paths from another SDK version — always use the user's installed SDK `examples/functions` tree.
