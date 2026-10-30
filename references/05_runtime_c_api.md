# RKNN Runtime C API Patterns

Use this file before writing C/C++ inference code. Start from the official or Model Zoo demo closest to the target SoC and model family.

## Contents

- [Always query model metadata](#always-query-and-log-model-metadata) — the standard init-time queries
- [Native attr query variants](#native-attr-query-variants-rknnrt-api-ref-43-9-12) — `NATIVE_INPUT_ATTR` / `NATIVE_NHWC_*` / aliases
- [First-layer input width alignment](#first-layer-input-width-alignment-compiler-op-list-51) — zero-copy `w_stride` table
- [mean/scale/quant backend selection](#meanscalequant-backend-selection-compiler-op-list-51) — NPU vs CPU fast/slow path
- [Output alignment per layout](#output-alignment-per-layout-compiler-op-list-52) — NHWC / NC1HWC2 / NCHW channel+HW align per SoC
- [NC1HWC2 native layout](#nc1hwc2-native-layout-user-guide-51) — C2 values per platform + NCHW/NHWC conversion code
- [Zero-copy input layout rules](#zero-copy-input-layout-rules-user-guide-523) — NCHW/NHWC/NC1HWC2/UNDEFINED packing
- [General API skeleton](#general-api-skeleton) — `rknn_init` → `rknn_inputs_set` → `rknn_run` → `rknn_outputs_get`
- [Zero-copy / external memory pattern](#zero-copy--external-memory-pattern) — `rknn_create_mem` + `rknn_set_io_mem`
- [RV1103/RV1106 style](#rv1103rv1106-style) — mini-runtime + RK2118 follow-up
- [Dynamic shape C API](#dynamic-shape-c-api) — `rknn_set_input_shapes` flow (not on RV110x/RK2118)
- [Multi-core and batch](#multi-core-and-batch) — `rknn_set_core_mask` / `rknn_set_batch_core_num`
- [Build/CMake reminders](#buildcmake-reminders) — runtime lib selection per arch
- [Matmul API](#matmul-api-rknn_matmul_apih) — separate ctx, dedicated set_io_mem
- [Custom op API](#custom-op-api-rknn_custom_oph) — last-resort op extension
- [`rknn_init` flags](#rknn_init-flags-rknnrt-api-ref-43) — the useful flag bits and when to use each
- [`rknn_create_mem` vs `rknn_create_mem2`](#rknn_create_mem-vs-rknn_create_mem2-rknnrt-api-ref-43) — cacheable vs non-cacheable allocation
- [Per-platform C API support matrix](#per-platform-c-api-support-matrix-rknnrt-api-ref-41) — which API works on which SoC
- [`rknn_init_extend` struct](#rknn_init_extend-struct-rknnrt-api-ref-42) — share-weight / zero-copy-model-buffer fields

## Always query and log model metadata

At init time, query and print:

- `RKNN_QUERY_SDK_VERSION`
- `RKNN_QUERY_IN_OUT_NUM`
- `RKNN_QUERY_INPUT_ATTR`
- `RKNN_QUERY_OUTPUT_ATTR`
- `RKNN_QUERY_CUSTOM_STRING`

For zero-copy, also query native attrs such as `RKNN_QUERY_NATIVE_INPUT_ATTR`, `RKNN_QUERY_NATIVE_OUTPUT_ATTR`, or native layout-specific output attrs.

Model Zoo demos cache `rknn_tensor_attr` arrays in the app context. Follow that pattern instead of hard-coding shapes.

### Native attr query variants (RKNNRT API Ref §4.3 #9-12)

The "native" queries have several aliases and variants — pick the right one for your postprocess:

| Query command | Returns | Layout |
|---|---|---|
| `RKNN_QUERY_NATIVE_INPUT_ATTR` | NPU-optimal input attr | NC1HWC2 or NHWC (whichever the hardware prefers) |
| `RKNN_QUERY_NATIVE_NC1HWC2_INPUT_ATTR` | **same result** as `NATIVE_INPUT_ATTR` in V2.3.2 | whatever the runtime chose |
| `RKNN_QUERY_NATIVE_NHWC_INPUT_ATTR` | **same result** as `NATIVE_INPUT_ATTR` in V2.3.2 | does **not** force NHWC for inputs |
| `RKNN_QUERY_NATIVE_OUTPUT_ATTR` / `..._NC1HWC2_OUTPUT_ATTR` | NPU-optimal output attr | NC1HWC2 or NHWC |
| `RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR` | **forces NHWC output** | NHWC |

The input-side `NHWC` query is commonly misunderstood: in V2.3.2 it is documented as returning the same result as `NATIVE_INPUT_ATTR`, not as a way to force NHWC. Only the **output** `NATIVE_NHWC_OUTPUT_ATTR` gives you NHWC directly and skips the NC1HWC2 unpack step. The query is listed as supported on all RKNPU2 platforms in V2.3.2, but it applies to 4-D outputs only.

## First-layer input width alignment (Compiler Op List §5.1)

Applies to **zero-copy API only** — the regular API has no alignment constraint. The constraint is per-input-width, queried from the runtime via `w_stride` (which is **fixed and cannot be modified**). Alignment depends on the **first-layer precision** as well as the SoC:

| SoC | First-layer precision | Width align when channel ∈ {1,3,4} | Width align when channel ∉ {1,3,4} |
|---|---|---|---|
| RK3566 / RK3568 | int8 | **8** | 1 |
| RK3566 / RK3568 | float16 | **4** | 1 |
| RK3588 | int8 | **16** | 1 |
| RK3588 | float16 | **8** | 1 |
| RK3562 | int8 | **16** | 1 |
| RK3562 | float16 | **8** | 1 |
| RK3576 / RV1126B | int8 | **16** | 1 |
| RK3576 / RV1126B | float16 | **8** | 1 |
| RV1103 / RV1106 | int8 | **16** | 1 |
| RV1103B / RV1106B | int8 | **8** | 1 |
| RK2118 | float16 | **4** | 1 |
| Non-4-D inputs (any SoC) | any | 1 | 1 |

For a 1920-wide RGB input on RK3588 with an int8 first layer, `w_stride` will be 1920 rounded up to a multiple of 16 (=1920, already aligned). A 1921-wide input gets `w_stride=1936` — the row stride differs from the logical width and the buffer must be padded accordingly. Allocate input mem with `size_with_stride`, not `size`.

### mean/scale/quant backend selection (Compiler Op List §5.1)

Whether the input mean/scale/quantization step runs on NPU or CPU depends on the **first layer's precision** AND the input dtype AND the input dimensionality:

| First-layer precision | Input dtype | Input dim | Backend | Notes |
|---|---|---|---|---|
| int8 | uint8 | 4-D | **NPU** | Fast path — normalize/quantize fused into NPU |
| int8 | int8 / float16 / other | 4-D | CPU | Slower; arrange to feed uint8 when possible |
| float16 | any | 4-D | CPU | Always CPU |
| any | any | non-4-D | CPU | Always CPU |

**RV1103 / RV1106 / RV1103B do NOT support the CPU path at all** — they cannot do mean/scale/quant at runtime. Either feed pre-normalized data or design the model so the first layer is int8 and the input is uint8.

### mean/scale for non-standard channel counts

- If `channel > 4`, mean/scale use only the **first value** (`mean[0]` / `scale[0]`) for all channels.
- If the model does not need mean/scale, set `mean_values=0` and `std_values=1` explicitly — leaving them unset is not the same as no-op.
- If the first layer is float16, there is no quantization step regardless of input dtype.

## Output alignment per layout (Compiler Op List §5.2)

When reading output via zero-copy API, the channel alignment depends on the layout you chose AND whether the model's last op is a conv-class op:

| SoC | Output dtype | Layout | Channel align (last layer = conv) | Channel align (last layer ≠ conv) | H×W align |
|---|---|---|---|---|---|
| RK3566 / RK3568 | int8 | NHWC | 8 | 8 | none |
| RK3566 / RK3568 | int8 | NC1HWC2 | 16 | 8 | 4 |
| RK3566 / RK3568 | float16 | NHWC | 4 | 4 | none |
| RK3566 / RK3568 | float16 | NC1HWC2 | 8 | 4 | 4 |
| RK3588 | int8 | NHWC | 16 | 16 | none |
| RK3588 | int8 | NC1HWC2 | 32 | 16 | 4 |
| RK3588 | float16 | NHWC | 8 | 8 | none |
| RK3588 | float16 | NC1HWC2 | 16 | 8 | 4 |
| RK3562 / RK3576 / RV1126B | int8 / float16 | NHWC | none | none | none |
| RK3562 / RK3576 / RV1126B | int8 | NC1HWC2 | 32 | 16 | 4 |
| RK3562 / RK3576 / RV1126B | float16 | NC1HWC2 | 16 | 8 | 4 |
| RV1103 / RV1106 | int8 | NC1HWC2 | 32 | 16 | 4 |
| RV1103 / RV1106 | int8 | NHWC | none | none | none |
| RV1103B / RV1106B | int8 | NC1HWC2 | 8 | 8 | 4 |
| RV1103B / RV1106B | int8 | NHWC | none | none | none |
| RK2118 | float16 | NC1HWC2 | 8 | 4 | 4 |

`NCHW` output never has channel alignment on any SoC. `UNDEFINED` (non-4-D output) never has alignment either. RV1103/1106 and RK2118 cannot produce NCHW or UNDEFINED outputs at all — they only emit NC1HWC2 or NHWC.

When the table says "32 align" for NC1HWC2 + conv last layer, the actual `C2` block size matches the values in the NC1HWC2 table earlier in this file — the runtime pads the channel dimension up to the next multiple of the align value.

## `rknn_init` flags (RKNNRT API Ref §4.3)

Defaults to `0`. Combine flags with bitwise OR. The most useful ones:

| Flag | Effect |
|---|---|
| `0` / `RKNN_FLAG_PRIOR_HIGH` | Default: runtime allocates weight + internal memory internally, context runs at high priority (same value as `0`). |
| `RKNN_FLAG_PRIOR_MEDIUM` / `RKNN_FLAG_PRIOR_LOW` | Lower the context scheduling priority. Distinct from the process `nice` value controlled by `RKNN_FLAG_DISABLE_PROC_HIGH_PRIORITY`. |
| `RKNN_FLAG_ASYNC_MASK` | C-side async single-thread mode: `rknn_outputs_get` returns the **previous** frame's result; not needed in multi-threaded apps. Toolkit2's Python `async_mode` is still documented as unsupported. |
| `RKNN_FLAG_FENCE_IN_OUTSIDE` / `RKNN_FLAG_FENCE_OUT_OUTSIDE` | External fence-fd synchronization (input fence in / output fence out) for display/GPU pipelines. Advanced; leave off unless you already use DRM fences. |
| `RKNN_FLAG_COLLECT_PERF_MASK` | Required to make `RKNN_QUERY_PERF_DETAIL` return per-layer timing. Without this flag, only `RKNN_QUERY_PERF_RUN` (total time) works. |
| `RKNN_FLAG_MEM_ALLOC_OUTSIDE` | ALL memory (input/output/weight/internal) is user-allocated via `rknn_set_io_mem` / `rknn_set_weight_mem` / `rknn_set_internal_mem`. Used by the multi-model shared-memory pattern in `references/11_dynamic_shape_and_mem_reuse.md`. |
| `RKNN_FLAG_INTERNAL_ALLOC_OUTSIDE` | Lighter: only internal tensor memory is user-managed (weight + I/O still runtime-allocated). Useful for memory reuse without the full external-allocation burden. |
| `RKNN_FLAG_SHARE_WEIGHT_MEM` | Share weights with another context (multi-resolution model pattern). Pair with `rknn_init_extend.ctx = <master ctx>`. **Cannot combine with dynamic shape.** |
| `RKNN_FLAG_COLLECT_MODEL_INFO_ONLY` | Initialize an empty context that can ONLY answer `rknn_query` (e.g. `MEM_SIZE`) — cannot run inference. Useful for pre-deployment memory planning. |
| `RKNN_FLAG_EXECUTE_FALLBACK_PRIOR_DEVICE_GPU` | When an op exceeds NPU spec, prefer GPU fallback (default is CPU). Only meaningful on SoCs with a GPU. |
| `RKNN_FLAG_ENABLE_SRAM` | Try to allocate internal tensors in **NPU SRAM** (RK3588: 956 KB usable, RK3576: 480 KB). See `references/13_advanced_features.md`. |
| `RKNN_FLAG_SHARE_SRAM` | Share another context's SRAM region; the current context must also set `RKNN_FLAG_ENABLE_SRAM`. |
| `RKNN_FLAG_DISABLE_PROC_HIGH_PRIORITY` | By default RKNN sets the process `nice` to -19. Set this to keep default priority (useful when the app shares a CPU with other latency-sensitive workloads). |
| `RKNN_FLAG_DISABLE_FLUSH_INPUT_MEM_CACHE` | Skip runtime's automatic input cache flush. **Caller must ensure the input tensor is cache-coherent before `rknn_run`** (e.g. via `rknn_mem_sync(..., TO_DEVICE)`). For pipelines where the input is never CPU-touched (camera → NPU direct). |
| `RKNN_FLAG_DISABLE_FLUSH_OUTPUT_MEM_CACHE` | Skip runtime's output cache flush. **Caller must call `rknn_mem_sync(..., FROM_DEVICE)` before reading `out_mem->virt_addr`**. For pipelines where the output goes to GPU/RGA rather than CPU. |
| `RKNN_FLAG_MODEL_BUFFER_ZERO_COPY` | Init from a model buffer allocated via `rknn_create_mem`/`rknn_create_mem2` — runtime does NOT copy the model buffer, saving memory. Requires filling `rknn_init_extend.{real_model_offset, real_model_size, model_buffer_fd, model_buffer_flags}`. Allocate with `RKNN_MEM_FLAG_ALLOC_NO_CONTEXT` if you need the buffer before any context exists. |

The cache-flush flags are the most error-prone: turning them on without manually doing the sync leads to intermittent "first frame wrong, later frames correct" bugs. Leave them off unless you have a measured cache-flush bottleneck.

## Multi-batch: `rknn_set_batch_core_num` vs `rknn_set_core_mask`

These are **two different APIs** (RKNNRT API Ref §4.3):

- `rknn_set_core_mask(ctx, mask)` — for **single-batch** models, picks which core(s) run inference.
- `rknn_set_batch_core_num(ctx, n)` — for **multi-batch** models (built with `rknn_batch_size>1`), sets how many cores participate in batch parallelism. RK3588/RK3576 only.

For a multi-batch model, you typically call `rknn_set_batch_core_num` once after init; `rknn_set_core_mask` is for single-batch core selection. They are not interchangeable.

## Output buffer allocation: `is_prealloc` modes (RKNNRT API Ref §4.3)

`rknn_outputs_get` accepts two modes per output:

- `is_prealloc=0` (default) — runtime allocates the output buffer; **must** be paired with `rknn_outputs_release` to free. Easiest.
- `is_prealloc=1` + user-provided `buf` — caller manages the buffer. Useful for pre-allocated output pools, but you must size `buf` correctly (use `output_attr.size * sizeof(float)` if `want_float=1`, else `output_attr.size`).

## `rknn_create_mem` vs `rknn_create_mem2` (RKNNRT API Ref §4.3)

- `rknn_create_mem(ctx, size)` — always cacheable.
- `rknn_create_mem2(ctx, size, alloc_flags)` — pick cacheable vs non-cacheable:
  - `RKNN_FLAG_MEMORY_CACHEABLE` / `RKNN_FLAG_MEMORY_FLAGS_DEFAULT` — same as `rknn_create_mem`.
  - `RKNN_FLAG_MEMORY_NON_CACHEABLE` — skip cache management entirely; **no `rknn_mem_sync` calls needed** for this buffer. Best for buffers written and read by different devices (NPU→GPU direct) with no CPU access.
- `rknn_create_mem2` also supports `RKNN_MEM_FLAG_ALLOC_NO_CONTEXT` — allows `ctx=NULL` so you can allocate NPU memory **before** any context is initialized. Free with `rknn_destroy_mem(ctx_any, mem)`.

## Per-platform C API support matrix (RKNNRT API Ref §4.1)

Selected entries — consult the table for the full list:

| API | RK35xx¹ / RV1126B | RK3576 / RK3588 | RV1103/1106 (B/non-B) | RK2118 |
|---|---|---|---|---|
| `rknn_init` / `rknn_destroy` / `rknn_query` / `rknn_run` | ✓ | ✓ | ✓ | ✓ |
| `rknn_inputs_set` / `rknn_outputs_get` / `rknn_outputs_release` | ✓ | ✓ | ✗ | ✗ |
| `rknn_set_core_mask` | ✗ | ✓ | ✗ | ✗ |
| `rknn_set_batch_core_num` | ✗ | ✓ | ✗ | ✗ |
| `rknn_dup_context` | ✓ | ✓ | ✗ | ✗ |
| `rknn_set_input_shapes` | ✓ | ✓ | ✗ | ✗ |
| `rknn_create_mem_from_phys` | ✓ | ✓ | ✓ | ✓ |
| `rknn_create_mem_from_fd` | ✓ | ✓ | ✓ | **✗** |
| `rknn_create_mem` / `rknn_create_mem2` / `rknn_destroy_mem` | ✓ | ✓ | ✓ | ✓ |
| `rknn_set_weight_mem` / `rknn_set_internal_mem` / `rknn_set_io_mem` | ✓ | ✓ | ✓ | ✓ |
| `rknn_mem_sync` | ✓ | ✓ | ✓ | ✓ |
| `rknn_matmul_*` (all) | ✓ | ✓ | ✗ | ✗ |
| `rknn_matmul_set_core_mask` | ✗ | ✓ | ✗ | ✗ |
| `rknn_register_custom_ops` / `rknn_custom_op_get_op_attr` | ✓ | ✓ | ✗ | ✗ |
| `rknn_B_normal_layout_to_native_layout` | ✓ | ✓ | ✗ | ✗ |

¹ RK35xx = RK3562 / RK3566 / RK3568.

Notable inversions: `rknn_create_mem_from_fd` works on RV110x but NOT on RK2118; `RKNN_QUERY_DEVICE_MEM_INFO` works on RV110x/RK2118 but NOT on RK35xx/RK3588.

## `rknn_query` for dynamic shape — RV110x/RK2118 unsupported

All `RKNN_QUERY_INPUT_DYNAMIC_RANGE`, `RKNN_QUERY_CURRENT_INPUT_ATTR`, `RKNN_QUERY_CURRENT_OUTPUT_ATTR`, `RKNN_QUERY_CURRENT_NATIVE_*` queries are unavailable on RV1103/1106/1103B/1106B/RK2118 — same restriction as `rknn_set_input_shapes` itself. See `references/11_dynamic_shape_and_mem_reuse.md`.

## `rknn_init_extend` struct (RKNNRT API Ref §4.2)

When passing a non-NULL `rknn_init_extend*` to `rknn_init`, fill the relevant fields:

| Field | When to set |
|---|---|
| `ctx` | For `RKNN_FLAG_SHARE_WEIGHT_MEM`: the master model's `rknn_context`. |
| `real_model_offset` | For path-based or zero-copy model init: offset of the real `.rknn` within the file/buffer. |
| `real_model_size` | Size of the real `.rknn` data. |
| `model_buffer_fd` | For `RKNN_FLAG_MODEL_BUFFER_ZERO_COPY`: fd from `rknn_create_mem2`. |
| `model_buffer_flags` | Flags from `rknn_create_mem2`. |
| `reserved` | Leave zeroed. |

## General API skeleton

Use on RK3562/RK3566/RK3568/RK3576/RK3588/RV1126B where `rknn_inputs_set` and `rknn_outputs_get` are supported.

```c
rknn_init(&ctx, model_data, model_size, 0, NULL);
rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(input_attr));
rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attr, sizeof(output_attr));
rknn_inputs_set(ctx, io_num.n_input, inputs);
rknn_run(ctx, NULL);
rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);
rknn_outputs_release(ctx, io_num.n_output, outputs);
rknn_destroy(ctx);
```

Do not use this path on RV1103/RV1106/RV1103B/RV1106B/RK2118.

## Zero-copy / external memory pattern

Use zero-copy when input buffers come from camera/decoder/DRM/RGA, when `fd` or physical address is available, or when platform support requires it.

```c
rknn_query(ctx, RKNN_QUERY_NATIVE_INPUT_ATTR, &in_attr, sizeof(in_attr));
rknn_query(ctx, RKNN_QUERY_NATIVE_OUTPUT_ATTR, &out_attr, sizeof(out_attr));
in_mem = rknn_create_mem(ctx, in_attr.size_with_stride);
out_mem = rknn_create_mem(ctx, out_attr.size_with_stride);
rknn_set_io_mem(ctx, in_mem, &in_attr);
rknn_set_io_mem(ctx, out_mem, &out_attr);
rknn_mem_sync(ctx, in_mem, RKNN_MEMORY_SYNC_TO_DEVICE);
rknn_run(ctx, NULL);
rknn_mem_sync(ctx, out_mem, RKNN_MEMORY_SYNC_FROM_DEVICE);
```

Allocate with `size_with_stride`, not only `size`, when using native attrs. Handle native output layouts such as `NC1HWC2`; convert before postprocess if the postprocess expects NCHW/NHWC.

## NC1HWC2 native layout (User Guide §5.1)

When `RKNN_QUERY_NATIVE_INPUT_ATTR` / `RKNN_QUERY_NATIVE_OUTPUT_ATTR` returns `fmt=NC1HWC2`, the data is laid out as `[N, C1, H, W, C2]` where `C1 = ceil(C / C2)`. `C2` is platform- and dtype-specific:

| SoC | int8 C2 | float16 C2 |
|---|---|---|
| RK2118 | N/A | 4 |
| RK3566 / RK3568 | 8 | 4 |
| RK3588 / RK3576 / RV1126B | 16 | 8 |
| RV1103B / RV1106B | 8 | N/A |
| RV1103 / RV1106 | 16 | 8 |
| RK3562 | 16 | 8 |

The trailing slots in each C2 block are padding zeros when `C % C2 != 0` (e.g. 13 channels on RK3568 → `C1=2, C2=8`, only the first 5 slots of the second C2 block are valid).

### NC1HWC2 → NCHW / NHWC conversion

User Guide §5.1 ships reference C routines for unpacking native output back to a layout the postprocess can consume. Skeleton (int8, NHWC target):

```c
int NC1HWC2_to_NCHW(const int8_t* src, int8_t* dst, const int* dims,
                    int channel, int h, int w) {
    int batch = dims[0], C1 = dims[1], C2 = dims[4];
    int hw_src = dims[2] * dims[3], hw_dst = h * w;
    for (int i = 0; i < batch; i++) {
        const int8_t* src_b = src + i * C1 * hw_src * C2;
        int8_t* dst_b = dst + i * channel * hw_dst;
        for (int c = 0; c < channel; ++c) {
            int plane = c / C2, offset = c % C2;
            const int8_t* src_c = src_b + plane * hw_src * C2;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    dst_b[c * hw_dst + y * w + x] =
                        src_c[C2 * (y * w + x) + offset];
        }
    }
    return 0;
}
```

For NHWC output, change the destination index to `dst_b[(y * w + x) * channel + c]`. Skip this work entirely if you can configure the output attr to `RKNN_TENSOR_NHWC` via `RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR` (4-D outputs; supported on all RKNPU2 platforms in V2.3.2).

## Zero-copy input layout rules (User Guide §5.2.3)

The layout returned by `RKNN_QUERY_NATIVE_INPUT_ATTR` dictates how the input buffer must be packed:

| Queried layout | Input shape | Dtype | Notes |
|---|---|---|---|
| `NCHW` | 4-D | bool, int64 | rare; pass data already in NCHW order |
| `NHWC` | 4-D, channels ∈ {1,3,4} | float32/float16/int8/uint8 | if `pass_through=1`, honour `w_stride` (row may need padding) |
| `NC1HWC2` | 4-D, channels ∉ {1,3,4} | float16/int8 | if `pass_through=0`, pack NHWC and let the runtime convert; if `pass_through=1`, pack NC1HWC2 yourself |
| `UNDEFINED` | non-4-D | any | no mean/std/layout conversion; pass raw tensor in model's native order |

You can override the queried dtype (`uint8→int8/float16`, `float32→int8/float16`, etc. — see User Guide tables 5-3/5-4) by editing `rknn_tensor_attr.type` before `rknn_set_io_mem`. Widening (e.g. `uint8→float32`) requires multiplying `attr.size` by the dtype-size ratio (4x in that case) — the runtime then falls back to CPU conversion, which costs performance.

For outputs, `RKNN_QUERY_NATIVE_NHWC_OUTPUT_ATTR` exists specifically to skip the NC1HWC2 unpack step when you need NHWC output directly.

## RV1103/RV1106 style

- Use `librknnmrt.so` and the `armhf-uclibc` runtime path.
- Model Zoo RV1106/RV1103 demos use zero-copy by default.
- Many examples call `rknn_init(&ctx, (char*)model_path, 0, 0, NULL)` instead of reading the model into host memory.
- Force input attr to `RKNN_TENSOR_UINT8` and `RKNN_TENSOR_NHWC` when matching the official RV110x zero-copy demos.
- RV1106 RGA requires DMA-allocated buffers in the Model Zoo image path.

**RK2118 follows the same mini-runtime style** (per the RK2118 Quick Start: the SDK uses "a mini runtime interface similar to RV1103/RV1106"). The same restrictions apply — no `rknn_inputs_set`/`rknn_outputs_get`, no dynamic shape, no matmul API. Library and headers live at `rt-thread/components/rknpu/{lib,include}` in the RT-Thread SDK. Full RK2118 workflow in `references/14_rk2118_rtos_deploy.md`.

## Dynamic shape C API

On supported platforms:

1. Query `RKNN_QUERY_INPUT_DYNAMIC_RANGE`.
2. Set chosen shapes with `rknn_set_input_shapes`.
3. Query `RKNN_QUERY_CURRENT_INPUT_ATTR` and `RKNN_QUERY_CURRENT_OUTPUT_ATTR`.
4. Allocate input/output buffers for current attrs.

Skip this pattern on RV110x/RK2118 because the relevant commands are unsupported.

## Multi-core and batch

- `rknn_set_core_mask(ctx, RKNN_NPU_CORE_...)` works on RK3588/RK3576 only.
- RK3588 has 3 NPU cores. RK3576 has 2; do not use core 2 or core mask 0_1_2.
- `rknn_set_batch_core_num` is only for RK3588/RK3576 multi-batch RKNN models.
- `rknn_batch_size` changes model input/output shapes at build time.

## Build/CMake reminders

- Include `rknn_api.h`; include `rknn_matmul_api.h` only for Matmul API; include `rknn_custom_op.h` only for custom ops.
- Link the correct runtime: `librknnrt.so` for normal RKNPU2, `librknnmrt.so` for RV110x, RK2118 runtime from the RK2118 SDK.
- Cross compile with the correct ABI: `aarch64`, `armhf`, or `armhf-uclibc`.
- Preserve Model Zoo utility dependencies when porting: `imageutils`, `fileutils`, `imagedrawing`, RGA, and JPEG settings can affect both correctness and performance.

## Matmul API (`rknn_matmul_api.h`)

Separate from the inference API and **not universally available**. Before generating matmul code:

1. Confirm the target SoC and SDK version expose `rknn_matmul_api.h` and the matmul runtime symbols. RV110x/RK2118 do not.
2. Use the dedicated creation/query/run flow (`rknn_matmul_create`, `rknn_matmul_set_io_mem`, `rknn_matmul_run`, `rknn_matmul_destroy`) — not `rknn_run`.
3. Matmul has its own supported shapes, data types (INT8/INT16/FP16/INT4 depending on platform), layout constraints, and quantization APIs. Treat the support table in `rknn_doc/MinerU_markdown_04_*.md` as authoritative, not the inference op list.
4. V2.3.2 also exposes `rknn_matmul_create_dynamic_shape` / `rknn_matmul_set_dynamic_shape` plus `rknn_matmul_set_quant_params` / `rknn_matmul_get_quant_params` on the full-runtime SoCs (not RV110x/RK2118). The older `rknn_matmul_create_dyn_shape` is deprecated.
5. Verify against the matmul demos in the user's RKNN-Toolkit2 install (`rknpu2/examples`) for the exact SDK release; signatures have changed between minor versions.

Only reach for matmul when an ordinary model cannot express the computation efficiently or the user explicitly asks for it.

## Custom op API (`rknn_custom_op.h`)

Last resort for ops RKNN cannot compile. **Full stage guide (ONNX → Toolkit2 `reg_custom_op` → board CPU/GPU demos): `references/21_custom_op.md`.**

Constraints:

- Target runtime must support custom ops — see the support matrix above; RV110x/RK2118 mini-runtime: **no**.
- Custom op registration is platform- and SDK-version-specific. Code written against V2.3.x may not link against V2.1.x.
- Default backend is **CPU**; selected demos show **GPU/OpenCL** custom ops. Neither matches NPU throughput for free.
- Prefer graph rewrite / CPU-side postprocess / `onnx_edit` layout fixes (`references/22_onnx_edit_and_graph_tools.md`) before custom ops.
- Mirror the closest demo in the **user's** install: Toolkit2 `examples/functions/custom_op/*` for conversion; `rknpu2/examples/rknn_custom_op_demo/{cpu,gpu,pytorch}` for board C.
