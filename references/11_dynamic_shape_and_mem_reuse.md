# Dynamic Shape and Memory Reuse

Use this file when the deployment scenario needs runtime-varying input shapes, multiple RKNN models sharing memory, or any of the `RKNN_FLAG_MEM_ALLOC_OUTSIDE` / `rknn_set_internal_mem` / `rknn_set_weight_mem` paths. Pair with the skeletons under `assets/`:

- `assets/c_api_dynamic_shape_template.c`
- `assets/c_api_internal_mem_reuse_template.c`

## Dynamic shape

### Conversion side

`rknn.config(...)` accepts `dynamic_input`. Each entry is one full input-shape set; the runtime may switch between them:

```python
rknn.config(
    target_platform='rk3588',
    mean_values=[[0, 0, 0]],
    std_values=[[255, 255, 255]],
    dynamic_input=[
        [[1, 3, 256, 256]],
        [[1, 3, 320, 320]],
        [[1, 3, 384, 384]],
    ],
)
```

- Every shape set must list ALL inputs in the same order as the model.
- Mixed dynamic dims inside a single input (e.g. only H varying) are supported but every combination must be enumerated at conversion time; RKNN does not infer new shapes at runtime.
- Dynamic shape is still marked **experimental** in the V2.3.2 `config()` table. Validate on the target runtime before shipping.
- Dynamic shape enlarges the `.rknn` file (multiple compiled sub-graphs) and memory footprint.

### MaxShape selection rule (User Guide §5.4.3)

Toolkit2 picks ONE shape from `dynamic_input` as the calibration target — the shape with the **largest sum of input element counts** across all inputs (not necessarily the first entry). It prints:

```
W config: The 'dynamic_input' function has been enabled,
   the MaxShape is dynamic_input[0] = [[1,224],[1,40]]!
```

Rules that follow from this:

- All shapes share the same quantization parameters derived from MaxShape.
- If the calibration dataset is `.jpg`/`.png`, Toolkit2 resizes each image to MaxShape automatically — different image sizes in `dataset.txt` are OK.
- If the calibration dataset is `.npy`, every entry must already be at MaxShape shape — Toolkit2 will NOT resize `.npy` data.
- `eval_perf()` and `eval_memory()` return numbers for MaxShape only. Benchmark other shapes by running them at runtime.
- For multi-input models, inputs are paired positionally across shape sets: input A's shape[k] is paired with input B's shape[k], not cross-combined.

### Init-time restrictions (User Guide §5.4.4.1)

A dynamic-shape `.rknn` cannot be combined with:

- **`RKNN_FLAG_SHARE_WEIGHT_MEM`** (the shared-weight multi-resolution pattern in `references/13_advanced_features.md`) — incompatible.
- **`rknn_dup_context`** (multi-thread context duplication) — also incompatible.

For multi-threaded serving of a dynamic-shape model, init one full context per thread.

### Runtime side

```c
rknn_input_range range; range.index = 0;
rknn_query(ctx, RKNN_QUERY_INPUT_DYNAMIC_RANGE, &range, sizeof(range));

for (each supported shape s in range) {
    /* copy range.dyn_range[s] into rknn_tensor_attr.dims */
    rknn_set_input_shapes(ctx, n_input, input_attrs_with_s);

    rknn_query(ctx, RKNN_QUERY_CURRENT_INPUT_ATTR,  &cur_in,  sizeof(cur_in));
    rknn_query(ctx, RKNN_QUERY_CURRENT_OUTPUT_ATTR, &cur_out, sizeof(cur_out));

    /* (re)allocate input/output buffers for current attrs */
    rknn_inputs_set(ctx, n_input, inputs);
    rknn_run(ctx, NULL);
    rknn_outputs_get(ctx, n_output, outputs, NULL);
    rknn_outputs_release(ctx, n_output, outputs);
}
```

Key rules:

- `RKNN_QUERY_INPUT_DYNAMIC_RANGE` returns the supported shape list. `rknn_set_input_shapes` MUST be called before every shape switch.
- After `rknn_set_input_shapes`, query `RKNN_QUERY_CURRENT_INPUT_ATTR` and `RKNN_QUERY_CURRENT_OUTPUT_ATTR` — these reflect the live shape, not the static `RKNN_QUERY_INPUT_ATTR`/`RKNN_QUERY_OUTPUT_ATTR`.
- Output buffer sizes change with the shape; do not reuse a fixed-size output allocation across shape switches without checking `cur_out.size`.
- Zero-copy + dynamic shape: allocate input/output mem with `size_with_stride` of the CURRENT attrs per shape, not the static ones.

### Platform gate

- Requires `rknn_server` + Runtime >= 1.5.0 for connected inference.
- NOT supported on RV1103/RV1106/RV1103B/RV1106B/RK2118 — `RKNN_QUERY_INPUT_DYNAMIC_RANGE` and `rknn_set_input_shapes` return errors there. For RV110x, build a single fixed shape per deployed model variant instead.
- On RK3588/RK3576, dynamic shape and `rknn_set_core_mask` can be combined; core mask still only takes effect on multi-core SoCs.

## External memory allocation (MEM_ALLOC_OUTSIDE)

`rknn_init` flags:

- `0` (default) — runtime allocates weight + internal buffers internally.
- `RKNN_FLAG_MEM_ALLOC_OUTSIDE` — **all** model memory (input, output, weight, internal) is user-managed. Weight/internal are supplied with `rknn_set_weight_mem` / `rknn_set_internal_mem`; I/O is supplied through `rknn_set_io_mem` (zero-copy). This is what lets multiple contexts share one DMA-able region.
- `RKNN_FLAG_INTERNAL_ALLOC_OUTSIDE` — lighter variant: only internal tensors are user-managed; weight and I/O stay runtime-allocated.

### Workflow

```c
ret = rknn_init(&ctx, model, model_size, RKNN_FLAG_MEM_ALLOC_OUTSIDE, NULL);

rknn_mem_size mem_sz;
rknn_query(ctx, RKNN_QUERY_MEM_SIZE, &mem_sz, sizeof(mem_sz));
/* mem_sz.total_weight_size, mem_sz.total_internal_size, mem_sz.total_dma_allocated_size */

rknn_tensor_mem* w_mem = rknn_create_mem(ctx, mem_sz.total_weight_size);
rknn_set_weight_mem(ctx, w_mem);

rknn_tensor_mem* i_mem = rknn_create_mem(ctx, mem_sz.total_internal_size);
rknn_set_internal_mem(ctx, i_mem);
```

### Multi-model reuse

To share ONE internal region across N contexts (typical: model ensemble, multi-camera, multi-stream):

1. Init every context with `RKNN_FLAG_MEM_ALLOC_OUTSIDE`.
2. Query `RKNN_QUERY_MEM_SIZE` for each; track `max(mem_sz[*].total_internal_size)`.
3. Allocate ONE shared region: `shared = rknn_create_mem(ctx[0], max_internal)`.
4. For each context, import the shared region by fd:
   ```c
   rknn_tensor_mem* per_ctx = rknn_create_mem_from_fd(
       ctx[n], shared->fd, shared->virt_addr, mem_sz[n].total_internal_size, 0);
   rknn_set_internal_mem(ctx[n], per_ctx);
   ```
5. Weight memory is usually model-specific; allocate separately per context with `rknn_set_weight_mem`.

Rules:

- The shared region must be at least as large as the largest model's `total_internal_size`.
- Do NOT `rknn_destroy_mem` the per-context `rknn_create_mem_from_fd` handles AND the original `shared` handle separately in a way that double-frees the underlying DMA buffer — destroy the per-context handles first, then destroy the original shared handle on its owning context.
- Concurrent `rknn_run` calls on different contexts sharing the same internal mem will alias — serialize them or use separate regions per stream.
- `RKNN_QUERY_MEM_SIZE` reports weight/internal (not input/output) sizes; use it to size the allocations for this flag.

### When to use it

- Multiple small models on a memory-constrained board (RK3562, RV1103/RV1106 with `librknnmrt.so`).
- Pipelines that already own a DMA allocator (camera, decoder, RGA) and want to avoid extra copies.
- Ensemble / cascaded models where one model's output feeds another and both can live in the same DMA region.

### What it is NOT

- Not the same as zero-copy I/O (`rknn_set_io_mem` over native input attrs). External allocation is about weight/internal memory; zero-copy is about input/output tensor memory. They compose.
- Not a substitute for building a smaller model. If memory is the bottleneck, also reduce input resolution, output tensor count, and move heavy postprocess to CPU.

## Common pitfalls

- Calling `rknn_set_input_shapes` with an attrs array that has stale `dims` from a previous shape — re-fill from `range.dyn_range[s]` every iteration.
- Allocating input/output mem once with the first shape's `size_with_stride` and reusing it across shapes — re-query current attrs.
- Mixing `RKNN_FLAG_MEM_ALLOC_OUTSIDE` with default-mode buffers — the flag covers input/output/weight/internal, so pair it with zero-copy I/O (`rknn_set_io_mem`) and supply weight/internal explicitly.
- Expecting dynamic shape on RV110x — it is unsupported there; build fixed-shape variants instead.
