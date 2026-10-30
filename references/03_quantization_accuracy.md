# Quantization and Accuracy Triage

Use this file when INT8/low-bit output differs from the original framework or when selecting quantization settings.

## Calibration dataset

- Use data representative of deployment lighting, aspect ratio, sensor pipeline, and input distribution.
- Verify resize/crop/letterbox/color/normalization exactly match training/export and C/Python inference.
- Suggested counts: `normal` 20-100 samples, `kl_divergence` 20-100, `mmse` 20-50, `gdq` more than 200.
- Use `.npy` calibration data for non-image tensors or when image loaders cannot reproduce the true preprocessing.

## Dtype and method selection

| Setting | Guidance |
|---|---|
| `w8a8` | Default V2.3.2 quantized dtype. Use first for normal INT8 deployment. **Not supported on RK2118** — use an FP16 build there. |
| `w4a16` | RK3576/RV1126B only. Consider `gdq` and `group{SIZE}` when accuracy matters. |
| `w8a16` | RK3562 only. `gdq` can improve weight accuracy. |
| `w16a16i`, `w16a16i_dfp` | RV1103/RV1106 only. Useful for RV-specific INT16 paths. |
| `w4a8` | Listed in the V2.3.2 `config()` table but marked **not implemented** — do not recommend it. |
| `channel` | Default and usually more accurate than `layer`. |
| `group32`...`group256` | Valid only for `w4a16`; often more accurate than `channel`. |
| `gdq` | Only valid with `w4a16` / `w8a16`; expect >200 calibration samples. |

## Triage sequence for bad quantized output

1. Run original framework output and FP16 RKNN output on the same tensor.
2. Compare simulator output before board output to separate conversion error from runtime/deployment error.
3. Verify RGB/BGR, NHWC/NCHW, mean/std, resize interpolation, crop/pad, and dtype.
4. Use one known image/data sample in `dataset.txt` and inference with the same sample.
5. Increase calibration samples to a representative range.
6. Run `accuracy_analysis()` and inspect the first layers with large cosine/euclidean error.
7. Try `kl_divergence`, `mmse`, `channel`, `group{SIZE}`, `auto_hybrid=True`, or manual hybrid quantization.
8. If early graph structure causes error, rewrite the exported graph or move fragile postprocess outside RKNN.
9. **Only after steps 6–7 are exhausted**: switch to QAT (Quantization-Aware Training). QAT needs the original training pipeline (PyTorch FX `prepare_qat_fx`, PaddleSlim QAT, or TFMOT) and the full training dataset; it is not a Toolkit2 knob. See `references/13_advanced_features.md` for the framework entry points Toolkit2 accepts and the trade-offs vs PTQ/hybrid.

## Hybrid quantization

```python
ret = rknn.hybrid_quantization_step1(dataset='./dataset.txt', proposal=True)
# Edit generated <model>.quantization.cfg.
ret = rknn.hybrid_quantization_step2(
    model_input='./model.model',
    data_input='./model.data',
    model_quantization_cfg='./model.quantization.cfg',
)
```

`hybrid_quantization_step1` parameters worth knowing (Toolkit2 API Ref §2.12.1):

- `proposal=True` — auto-generate suggested layers to switch to FP16 in the `.quantization.cfg`. Expensive; defaults to using only the first dataset image.
- `proposal_dataset_size=N` — number of dataset images to use for the proposal (default 1). Raise for noisier distributions.
- `custom_hybrid=[[in_name, out_name], ...]` — explicitly select hybrid-quantized subgraphs by tensor name (read from the generated `.model` via Netron). Use this when the proposal picks too many or too few layers.

In V2.3.2, `custom_quantize_layers` accepts only **`float16`** as the per-layer override; other values documented in older SDKs are not supported. The `quantize_parameters` block (`qtype` / `qmethod` / `dtype` / scale / zero-point) is generated for inspection — do not hand-edit it. Combine the cfg with `accuracy_analysis()`; do not blindly trust the proposal list. `auto_hybrid=True` is only supported for `w8a8` quantization; `w4a16`/`w8a16` need the manual step1/step2 path.

## `accuracy_analysis()` output layout (Toolkit2 API Ref §2.13)

Must be called after `build()` or `hybrid_quantization_step2()` — never after `load_rknn()`. If `target` is unset and the source model is a QAT-quantized model, the call fails (QAT requires the original framework model).

```python
rknn.accuracy_analysis(
    inputs=['./dog_224x224.jpg'],   # jpg/png/bmp/npy
    output_dir='./snapshot',        # default './snapshot'
    target=None,                    # None = simulator only; set SoC for board+simulator
    device_id=None,
)
```

The output directory structure:

```
snapshot/
├── golden/              # float reference — every layer's output from the FP model
├── simulator/           # quantized model layer outputs on the PC simulator (float32)
├── runtime/             # only if target=<SoC>: quantized model layer outputs from the board NPU (float32)
└── error_analysis.txt   # per-layer cosine + euclidean distance table
```

`error_analysis.txt` columns to look at (the names are slightly misleading):

- **`simulator_error / entire`** — running the full quantized model vs golden, computed at this layer's output. Errors accumulate down the graph.
- **`simulator_error / single`** — feeding the golden input to this layer only, comparing quantized vs golden output. Isolates the layer's own error from upstream accumulation. **Start debugging here**: a layer with small `single` but large `entire` is fine itself; the problem is upstream.
- **`runtime_error / entire`** and **`runtime_error / single`** — same idea but comparing board NPU output to golden. Large `runtime_error` with small `simulator_error` points to a board-side Runtime bug worth reporting to Rockchip.

## Model Zoo accuracy traps

- YOLO demos expect model-specific postprocess. Missing sigmoid, wrong anchors, or mismatched NMS/box thresholds can look like quantization failure.
- Model Zoo benchmark numbers exclude preprocessing and postprocessing time.
- Different image readers (`cv2`, `stbi`, `turbojpeg`) can change MAP slightly.
