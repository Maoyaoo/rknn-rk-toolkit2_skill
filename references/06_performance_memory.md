# Performance and Memory Optimization

Measure correctness first, then optimize. Model Zoo benchmark values usually measure only `rknn_run`, not preprocessing or postprocessing.

## Measurement checklist

1. Verify output correctness with one fixed input.
2. Use `init_runtime(perf_debug=True)` + `eval_perf()` where supported (`perf_debug` is an init-time switch; `eval_perf()` only prints).
3. Use `eval_memory()` for weight, internal tensor, other memory, and total memory.
4. In C/C++, query SDK version and performance details where supported.
5. Fix CPU/NPU/DDR frequency before comparing with Model Zoo numbers.
6. On RK3588/RK3576, bind performance tests to big CPU cores when CPU postprocess matters.

### Frequency, driver, and load (User Guide §8.1.1)

Quick "performance mode" — pin every governor in `/sys` to `performance`:

```bash
echo performance | tee $(find /sys/ -name *governor) 2>/dev/null
```

Or use the per-SoC `scaling_frequency.sh` from rknn_model_zoo. **NPU load** (busy-time ratio, NOT MAC utilization):

```bash
cat /sys/kernel/debug/rknpu/load        # RK35xx / RK3588
cat /proc/debug/rknpu/load              # alternative path
```

Low NPU load means the NPU is idle waiting for input — the bottleneck is pre/post-processing or host→NPU data movement, not the model. Raise throughput by multi-threading the input pipeline or moving preprocess to RGA.

## Operator-level design rules (User Guide §8.5)

### Channel alignment per SoC

For best DDR efficiency, design Conv input/output channels as multiples of these numbers. Non-aligned channels work but cost extra redundant ops:

| SoC | Dtype | Conv in_ch | Conv out_ch | DW Conv ch | Other OP ch |
|---|---|---|---|---|---|
| **RK3566 / RK3568** | Int8 | 32 | 16 | 32 | 8 |
|  | Int16 / Float16 / BFloat16 | 16 | 8 | 16 | 4 |
| **RK3588 / RK3576 / RV1126B** | Int8 | 32 | 32 | 64 | 16 |
|  | Int16 / Float16 / BFloat16 | 32 | 16 | 32 | 8 |
|  | TFloat32 | 16 | 16 | 16 | 4 |
| **RV1103 / RV1106** | Int8 | 32 | 16 | 32 | 16 |
|  | Int16 | 16 / 32¹ | 16 / 8¹ | 16 | 8 |
|  | Float16 / BFloat16 | 32 | 8 | 16 | 8 |
|  | TFloat32 | 16 | 8 | 8 | 4 |
| **RK3562** |  | Height × Width > 1 → 4-align |  |  |  |

¹ RV110x Int16 has two valid configs depending on op; consult the compiler support PDF for the exact row.

### Conv size vs MAC utilization heuristics (User Guide §8.5.2)

When evaluating why a specific Conv underperforms:

- **Input channel** < 256 → high utilization; > 512 → utilization drops progressively.
- **Weight tensor size** `KH × KW × KC × type_bytes` < **6 KB** → high utilization; over this, drops fast.
- **`Ksize / Kstride` ratio** — bigger is better. `(K=3, S=1)` beats `(K=2, S=1)`.
- **Output `H × W`** < 16 → utilization drops.
- **Output channel** larger → utilization higher.
- Width >> Height is more DDR-friendly than square (e.g. 1×12 conv beats 3×4 conv at equal FLOPs).

### Low-efficiency ops to avoid in the model (User Guide §8.5.2)

| Data-movement | Shape-change | Non-ReLU activations |
|---|---|---|
| Transpose, Reshape, Split, Concat | Resize, Tile, Pooling, Pad | Sigmoid, Tanh, Softplus, Hardswish |

These compile but spend cycles on data rearrangement rather than MACs. If a model has many of them in the hot path, prefer graph rewrite (slice→conv, fuse sequences) — see `references/07_operator_support.md`.

## Multi-core NPU tuning (RK3588 3-core, RK3576 2-core)

Beyond setting `core_mask`, two extra knobs matter for multi-core throughput (User Guide §5.3.3):

1. **Pin NPU IRQs to the CPU big core** running your app — eliminates cross-core interrupt dispatch:

   ```bash
   # Find NPU IRQ numbers
   interrupts=$(cat /proc/interrupts | grep npu)
   # Pin each to CPU4 (RK3588 big core)
   for irq in $irq1 $irq2 $irq3; do
       echo 4 > /proc/irq/$irq/smp_affinity_list
   done
   # taskset takes a hex mask: 0x10 = CPU4, written as "10"
   taskset 10 ./rknn_yolov5_demo ...
   ```

2. **Only these op types get multi-core task splitting** in V2.3.2: `Conv`, `DepthwiseConvolution`, `Add`, `Concat`, `Relu`, `Clip`, `Relu6`, `ThresholdedRelu`, `PRelu`, `LeakyRelu`. Other ops stay single-core even under `NPU_CORE_0_1_2` — don't expect a speedup on a model composed mostly of, say, `Resize` and `Sigmoid`.

Check actual per-core workload by running with `RKNN_LOG_LEVEL=4` (C) or `RKNN(verbose=True)` + `eval_perf()` (Python) and reading the `WorkLoad(0/1/2)` column (RK3576 prints `WorkLoad(0/1)`). A row like `50%/50%/0%` means cores 0 and 1 split the layer; `100%/0%/0%` means single-core.

## Optimization levers

- Quantize to `w8a8` or a platform-supported low-bit mode.
- Move NMS, decode, mask generation, text/audio postprocess, or unsupported ops outside RKNN when they are postprocess-friendly.
- Use RGA/camera/decoder zero-copy paths for high-throughput image pipelines.
- Use native layout/zero-copy to avoid avoidable input/output copies.
- Use `want_float=0` when postprocess can consume quantized outputs; large float outputs add conversion cost.
- Use RK3588/RK3576 core masks, multi-context, or batch/core settings only after checking platform support.
- Use `rknn_batch_size` only when the app can handle changed shapes and batched postprocess (see `references/13_advanced_features.md` for the multi-batch internals: weight is reused via `rknn_dup_context`, batch size should be a multiple of 3 on RK3588 and 2 on RK3576).
- Confirm the board firmware has reserved NPU SRAM (RK3588: 956 KB usable, RK3576: 480 KB). SRAM allocation is a BSP/device-tree knob, not a Runtime API; an OEM build may have given the SRAM to another IP and silently lowered NPU throughput.

## Memory failures

For RV1103/RV1106/RV1103B/RV1106B:

- Prefer smaller model and lower input resolution.
- Quantize when accuracy allows.
- Reduce output tensors and move heavy postprocess outside the model.
- Use `librknnmrt.so` from the correct SDK path.
- Upgrade `rknn_server`/Runtime to at least 2.1.0 for connected debugging.
- Stop other board applications before connected inference or accuracy analysis. Rockchip firmware ships `RkLunch-stop.sh` for this; it stops non-essential services in one step.
- **Kill `rkipc` before loading the model.** These boards ship with `rkipc` (the camera/ISP service) running by default. On a memory-tight board it causes `RKNN_ERR_MODEL_INVALID` at `rknn_init` / `init_runtime`. Stop it explicitly:

  ```bash
  adb shell 'ps | grep rkipc'             # find the PID
  adb shell 'kill -9 <pid>'               # or: killall rkipc
  ```

  Source: RV1106/RV1103 Quick Start §6.3. Only stop it when you actually need the NPU — `rkipc` runs the camera pipeline, so killing it disables camera input.

For RK3588/RK3576:

- Consider single-core mode only when memory/model size matters more than throughput.
- Consider internal memory reuse or zero-copy examples from `rknpu2/examples` for large pipelines.
