# RKNN Advanced Features

Cross-cutting RKNN-Toolkit2 / RKNNRT features that don't fit into the conversion, runtime API, or quantization docs. Source: RKNN SDK V2.3.2 User Guide chapters 5.6–5.13, 9.5.

Reach for this file when the user asks about: model encryption, multi-batch performance, sharing weights across resolution variants, cacheable memory sync, NPU SRAM, or QAT (Quantization-Aware Training).

Related advanced topics live in dedicated files (do not duplicate here):

- Custom operators → `references/21_custom_op.md`
- `onnx_edit` / Toolkit2 `examples/functions` map → `references/22_onnx_edit_and_graph_tools.md`
- AutoSparsity / `sparse_infer` / `model_pruning` → `references/23_sparsity_and_toolkit_functions.md`

## Model encryption

`rknn.export_rknn()` produces a model whose graph can be opened in Netron and whose weights are recoverable. To protect IP, encrypt the exported RKNN:

```python
# export_encrypted_rknn_model(input_model, output_model=None, crypt_level=1)
ret = rknn.export_encrypted_rknn_model("input.rknn", "encrypt.rknn", 1)
```

- `output_model` defaults to `<input>.crypt.rknn`; `crypt_level` is 1, 2, or 3 (default 1).
- The encrypted model loads and runs identically to the unencrypted one — no runtime-side changes.
- Higher `crypt_level` = stronger protection + slower decryption. Level 1 is the default.
- Supported platforms: **RK3562 / RK3566 / RK3568 / RK3576 / RK3588 / RV1126B**. Not RV1103/RV1103B/RV1106/RV1106B/RK2118.
- V2.3.2 documents only the three `crypt_level` values; no password parameter is documented — do not promise password protection.
- Encryption is applied AFTER `export_rknn`. Build the model normally first.

## Multi-batch internals

`rknn.build(rknn_batch_size=N)` enlarges input/output shapes by N. What happens at runtime:

- The runtime calls `rknn_dup_context` internally to clone the internal-memory context N-1 times. **Weight is NOT duplicated** — all clones share the same weight memory.
- RK3588: only 3 NPU cores exist, so `rknn_dup_context` is called at most twice (for batch >= 3). RK3576 has 2 cores, so at most once.
- Each clone's `core_mask` is set to 0 (auto-schedule). `rknn_run()` creates a thread pool that runs the N clones concurrently across cores.

### Tuning guidance

- RK3588: pick `rknn_batch_size` as a multiple of **3**.
- RK3576: pick a multiple of **2**.
- Always pair multi-batch with the **zero-copy** interface — `rknn_inputs_set`/`rknn_outputs_get` would erase most of the throughput benefit.
- Per-batch input/output offset: `offset_batch_k = (total_input_size / rknn_batch_size) * k`. Use this to scatter/gather batch slices in/out of the zero-copy mem.

### When multi-batch is the wrong tool

- Latency-sensitive single-stream pipelines: the thread pool adds overhead and the per-batch slices complicate postprocess.
- RV1103/RV1106/RK2118: no multi-core, so multi-batch does not parallelize; reduce input resolution instead.

## Multiple resolution models sharing weights

When you ship the same network at several input resolutions, build them so they share one weight blob:

1. **Master model**: `rknn.config(..., remove_weight=False)` — its `.rknn` file contains the weights.
2. **Slave models**: `rknn.config(..., remove_weight=True)` — their `.rknn` files omit conv-class weights.
3. At runtime, init the master first, then init each slave with the share-weight flag pointing at the master context:

```c
rknn_context ctx_master, ctx_slave;
rknn_init(&ctx_master, model_master, 0, 0, NULL);

rknn_init_extend extend;
extend.ctx = ctx_master;
rknn_init(&ctx_slave, model_slave, 0, RKNN_FLAG_SHARE_WEIGHT_MEM, &extend);
```

Notes:

- This pattern is **superseded by dynamic shape** (see `references/11_dynamic_shape_and_mem_reuse.md`) on SDK > 1.5.0. Prefer dynamic shape unless you must ship distinct fixed-resolution `.rknn` files.
- The slave model must be the same architecture as the master; only the input HxW differs.
- Combine with `RKNN_FLAG_MEM_ALLOC_OUTSIDE` only after testing — both flags interact with internal allocation.

## Cacheable memory consistency

When CPU and NPU both access the same DMA buffer with CPU caching enabled, the CPU cache can hold a stale view of DDR. `rknn_mem_sync` flushes/invalidates the cache in the requested direction:

```c
typedef enum {
    RKNN_MEMORY_SYNC_TO_DEVICE   = 0x1,                     /* CPU wrote, NPU will read */
    RKNN_MEMORY_SYNC_FROM_DEVICE = 0x2,                     /* NPU wrote, CPU will read  */
    RKNN_MEMORY_SYNC_BIDIRECTIONAL = TO_DEVICE | FROM_DEVICE,
} rknn_mem_sync_mode;

int rknn_mem_sync(rknn_context ctx, rknn_tensor_mem* mem, rknn_mem_sync_mode mode);
```

Rules:

- After CPU writes input pixels into `in_mem->virt_addr` and before `rknn_run`, call `rknn_mem_sync(ctx, in_mem, RKNN_MEMORY_SYNC_TO_DEVICE)`.
- After `rknn_run` returns and before CPU reads `out_mem->virt_addr`, call `rknn_mem_sync(ctx, out_mem, RKNN_MEMORY_SYNC_FROM_DEVICE)`.
- Use `BIDIRECTIONAL` only when you genuinely don't know the direction — the extra invalidation costs cycles.
- Pass a context from the same driver; the mem must have been allocated by an RKNN API (`rknn_create_mem`/`rknn_create_mem2`/`rknn_create_mem_from_*`).

The skill's zero-copy and RGA templates (`assets/c_api_zero_copy_skeleton.c`, `assets/c_api_rga_zero_copy_template.cpp`) already include these calls; this section explains why.

## NPU SRAM

Some SoCs expose internal SRAM that can be reserved for RKNPU to reduce DDR bandwidth:

| SoC | Total SRAM | Usable by IPs |
|---|---|---|
| RK3588 | 1 MB | 956 KB |
| RK3576 | 512 KB | 480 KB |

- The reservation is firmware / device-tree controlled, not a Runtime API call from the app. The application benefits transparently once the BSP allocates SRAM to RKNPU.
- If a customer reports inconsistent perf vs. Model Zoo numbers, confirm with them whether their firmware reserves SRAM for NPU; an OEM build may have given the SRAM to another IP.
- SRAM is shared with other IPs (VPU, GPU, RGA); allocating it to NPU trades away from those.

### SRAM-related init flags (RKNNRT API Ref §4.3)

Two flags control NPU SRAM usage at the context level:

- `RKNN_FLAG_ENABLE_SRAM` — try to allocate the context's internal tensors in SRAM.
- `RKNN_FLAG_SHARE_SRAM` — share another context's SRAM region (requires `ENABLE_SRAM` on the current context too). Useful when multiple models in one process would otherwise each carve out SRAM.

Query the SRAM footprint after `rknn_init` with `RKNN_QUERY_MEM_SIZE` — `rknn_mem_size` returns `total_sram_size` and `free_sram_size` (RK3588 only; zero on other SoCs or when SRAM is not enabled).

## Cache-management init flags (RKNNRT API Ref §4.3)

Two flags trade off automatic cache coherency for lower-latency I/O. Both are advanced — turning them on without doing the manual sync work causes silent data corruption:

- `RKNN_FLAG_DISABLE_FLUSH_INPUT_MEM_CACHE` — runtime will NOT flush the input cache before `rknn_run`. The caller must ensure cache coherency, e.g. by writing input via DMA from RGA/decoder, or by calling `rknn_mem_sync(ctx, in_mem, RKNN_MEMORY_SYNC_TO_DEVICE)` after the CPU write.
- `RKNN_FLAG_DISABLE_FLUSH_OUTPUT_MEM_CACHE` — runtime will NOT invalidate the output cache after `rknn_run`. Before the CPU reads `out_mem->virt_addr`, the caller MUST call `rknn_mem_sync(ctx, out_mem, RKNN_MEMORY_SYNC_FROM_DEVICE)`. For outputs that are consumed by GPU or RGA (no CPU read), this flag is pure win.

Pair with `rknn_create_mem2(..., RKNN_FLAG_MEMORY_NON_CACHEABLE)` for buffers that are never CPU-touched — non-cacheable memory needs no `rknn_mem_sync` at all.

## Zero-copy model buffer init (RKNNRT API Ref §4.3)

`RKNN_FLAG_MODEL_BUFFER_ZERO_COPY` skips the runtime's internal copy of the model buffer, saving runtime memory equal to the model size. The flow:

```c
/* 1. Allocate NPU memory BEFORE any context exists */
rknn_tensor_mem* model_mem = rknn_create_mem2(
    0,                    /* ctx = NULL is OK because of the flag below */
    model_size,
    RKNN_MEM_FLAG_ALLOC_NO_CONTEXT);
memcpy(model_mem->virt_addr, model_data, model_size);

/* 2. Init the context zero-copy */
rknn_init_extend ext;
memset(&ext, 0, sizeof(ext));
ext.real_model_offset = 0;
ext.real_model_size = model_size;
ext.model_buffer_fd = model_mem->fd;
ext.model_buffer_flags = model_mem->flags;

rknn_init(&ctx, model_mem->virt_addr, model_size,
          RKNN_FLAG_MODEL_BUFFER_ZERO_COPY, &ext);

/* 3. Use the context normally... */

/* 4. Free mem while a valid context still exists, then destroy the context */
rknn_destroy_mem(ctx, model_mem);
rknn_destroy(ctx);
```

The model buffer must stay alive for the lifetime of the context — do not free it before the last `rknn_run`. `rknn_destroy_mem` also needs a valid context; if the buffer was allocated with `RKNN_MEM_FLAG_ALLOC_NO_CONTEXT` before any context existed, pass any live context at free time.

## Quantization-Aware Training (QAT) — overview

QAT inserts FakeQuantize nodes into the model during training so the loss function sees quantization error. Result: a model that quantizes more cleanly than post-training quantization (PTQ) allows.

### When to consider QAT

Only after both gates fail:

1. Run the accuracy triage in `references/03_quantization_accuracy.md` and confirm PTQ cannot meet the accuracy bar.
2. Try `hybrid_quantization_step1/step2` (and `auto_hybrid`) per `references/03_quantization_accuracy.md` and confirm it also falls short on accuracy or perf.

QAT requires retraining with the full training dataset and framework-specific quantization-aware code; it is not a Toolkit2 button. Consider it last because:

- QAT models are slightly slower at inference than PTQ when the model contains ops QAT does not support.
- It needs the original training pipeline; cannot be applied to a downloaded ONNX alone.

### Framework entry points Toolkit2 accepts

- **PyTorch** (recommended path): RKNN supports models produced by the FX graph API, specifically `prepare_qat_fx` and friends. Older eager-mode QAT (`torch.quantization.prepare_qat`) is not accepted by Toolkit2 V2.3.2.
- **PaddlePaddle**: PaddleSlim `quanter.qat`.
- **TensorFlow**: TFMOT `tfmot.quantization.keras.quantize_model`.

After QAT produces a model, run the normal `load_pytorch` / `load_tensorflow` → `build(do_quantization=True, dataset=...)` flow; Toolkit2 will pick up the learned quantization parameters from the QAT graph. Treat QAT as PTQ-with-better-starting-params; you still need a calibration dataset.

### PyTorch qconfig — RKNN-specific knobs (User Guide §6.4.4)

The default PyTorch FX qconfig needs two RKNN-specific adjustments or the exported QAT model will quantize suboptimally:

```python
from torch.quantization import quantize_fx, QConfig, FakeQuantize, \
    MovingAverageMinMaxObserver, MovingAveragePerChannelMinMaxObserver

qconfig = QConfig(
    activation=FakeQuantize.with_args(
        observer=MovingAverageMinMaxObserver,
        quant_min=0, quant_max=255,
        reduce_range=False),          # ← RKNN: False (default True). Enables full -128..127 range.
    weight=FakeQuantize.with_args(
        observer=MovingAveragePerChannelMinMaxObserver,
        quant_min=-128, quant_max=127,
        dtype=torch.qint8,
        qscheme=torch.per_channel_affine,  # ← RKNN: per_channel_affine (default per_channel_symmetric).
        reduce_range=False))              # Symmetric forces zp=0; RKNN supports non-zero zp.

qconfig_dict = {"": qconfig}
model_qat = quantize_fx.prepare_qat_fx(float_model, qconfig_dict)
# ... training loop ...
model_qat = quantize_fx.convert_fx(model_qat)
```

Why:

- `reduce_range=False` — RKNN hardware supports the full INT8 range `-128..127`. Default `reduce_range=True` would clip to `-64..63` and waste half the range.
- `qscheme=torch.per_channel_affine` — RKNN hardware supports a non-zero zero-point per channel. Default `per_channel_symmetric` forces `zp=0` and loses accuracy on weight distributions that aren't centered.

Recommended PyTorch versions for QAT export: **1.10 ~ 1.13.1**. Lower (1.6.0) for non-QAT models.

### QAT gotchas at inference

- **PyTorch verification mismatch**: when comparing RKNN output to the source PyTorch model, set `engine='qnnpack'` on the PyTorch side — RKNN's compute order matches qnnpack, not the default fbgemm.
- **Unquantizable ops in QAT models**: if a float op sits between two quantized ops, Toolkit2 inserts extra quantize/dequantize ops around it, adding runtime cost. If the float op is at the model boundary, prefer moving it to CPU postprocess entirely.
- **Adjusted quantization params**: some PyTorch ops (e.g. Sigmoid) store placeholder quantization params that get rewritten at inference time. Toolkit2 detects this and applies the same adjustment during conversion — but if you observe a specific Sigmoid behaving oddly, this is why.

## Pitfalls

- Encrypting a model that targets RV1103/1106/RK2118 — those SoCs are not in the supported list.
- Calling `rknn_batch_size=N` then using `rknn_inputs_set` with a buffer sized for batch=1 — sizes change by N, allocate accordingly.
- Forgetting `rknn_mem_sync` after CPU writes before `rknn_run` — the most common cause of "first frame wrong, later frames correct" symptoms.
- Expecting SRAM to fix a perf problem without checking the firmware reservation — it's a BSP knob, not an app knob.
- Jumping to QAT before exhausting `auto_hybrid` / manual hybrid quantization — QAT is the most expensive option and often unnecessary.
