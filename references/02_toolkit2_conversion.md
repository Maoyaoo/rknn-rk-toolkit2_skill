# RKNN-Toolkit2 Conversion Guide

Use this file when writing or reviewing conversion scripts. Match the installed Toolkit2 version first; these notes target RKNN SDK V2.3.2.

## Contents

- [Minimum conversion flow](#minimum-conversion-flow) — `config → load_* → build → export_rknn`
- [Host environment](#host-environment) — Toolkit2 install, Python / ARM64 / Docker notes
- [Recommended source-framework versions](#recommended-source-framework-versions-toolkit2-api-ref-13) — PyTorch / ONNX / TF / Caffe / DarkNet ranges
- [`rknn_convert` YAML wrapper](#rknn_convert-yaml-wrapper-user-guide-316) — CLI batch conversion without Python
- [Config parameters to handle explicitly](#config-parameters-to-handle-explicitly) — full table incl. `op_target`, `remove_reshape`, `enable_flash_attention`, etc.
- [Build choices](#build-choices) — `do_quantization`, `rknn_batch_size`, `auto_hybrid`
- [Multi-input and data format](#multi-input-and-data-format) — dataset.txt + per-input `data_format`
- [Connected inference/evaluation](#connected-inferenceevaluation) — `init_runtime` / `inference` / `eval_perf` / `eval_memory`
- [`init_runtime()` parameter reference](#init_runtime-parameter-reference-toolkit2-api-ref-27) — incl. `async_mode`, `fallback_prior_device`, `core_mask`
- [`inference()` parameter reference](#inference-parameter-reference-toolkit2-api-ref-28) — `inputs_pass_through`
- [`load_rknn()` constraints](#load_rknn-constraints-toolkit2-api-ref-26) — what you cannot do after loading a `.rknn`
- [`get_sdk_version()` returns three versions](#get_sdk_version-returns-three-versions-toolkit2-api-ref-211) — API + rknn_server + rknnrt
- [`list_devices()` returns two lists](#list_devices-returns-two-lists-toolkit2-api-ref-214) — adb + ntb
- [Auto-generate C++ deployment scaffold: `codegen()`](#auto-generate-c-deployment-scaffold-codegen-toolkit2-api-ref-217)

## Minimum conversion flow

1. Create `RKNN(verbose=True, verbose_file='rknn_build.log')`.
2. Call `rknn.config(...)` with explicit `target_platform` and model-accurate preprocessing.
3. Load the model with `load_onnx`, `load_pytorch`, `load_tensorflow`, `load_tflite`, `load_caffe`, or `load_darknet`.
4. Call `rknn.build(...)`.
5. Call `rknn.export_rknn(...)`.
6. Always call `rknn.release()`.

## Host environment

- Toolkit2 V2.3.2 docs recommend **Python 3.8**; wheels are `cpXX`-specific, so match the interpreter ABI.
- Install from pip (`pip install rknn-toolkit2 -i https://pypi.org/simple`) or from the local wheel under `rknn-toolkit2/packages/{x86_64,arm64}/`.
- Verify with `python -c "from rknn.api import RKNN"` and `pip show rknn-toolkit2`.
- **x86_64** Toolkit2 supports Caffe / TensorFlow / TFLite / ONNX / DarkNet / PyTorch, connected debugging, and Docker.
- **ARM64** Toolkit2 converts ONNX/PyTorch only, has no Docker image, and does not provide adb-connected debugging; it is meant to run conversion/debug locally on the board.
- Connected debugging from x86_64 needs a matching board-side `rknn_server` and Runtime. ARM64 uses the local board Runtime without `rknn_server`.
- `ImportError: libGL.so.1` on WSL: see `references/08_wsl.md`.

## Recommended source-framework versions (Toolkit2 API Ref §1.3)

Match these versions when exporting the source model. Ranges are for **Toolkit2 V2.3.2**; older Toolkit2 releases support narrower ranges (e.g. Toolkit2 1.4.x–1.5.2 only accepts PyTorch 1.6.0–1.10.1 / ONNX 1.7.0–1.10.0).

| Framework | Toolkit2 V2.3.2 supported range | Notes |
|---|---|---|
| Caffe | **1.0** (protocol from BVLC commit `828dd10` + RKNN extensions) | ARM64 Toolkit2 does NOT support Caffe |
| TensorFlow | **1.12.0 ~ 2.14.0** (recommended 2.6.2 or 2.8.0) | ARM64 Toolkit2 does NOT support TF |
| TensorFlow Lite | Schema version **3** | TFLite schema versions are mutually incompatible — match the schema used at export |
| ONNX | **1.7.0 ~ 1.17.0** | ARM64 Toolkit2 supports ONNX |
| DarkNet | commit **`810d7f7`** (master branch) | ARM64 Toolkit2 does NOT support DarkNet |
| PyTorch | **1.10.1 ~ 2.4.0** (torchscript `.pt` via `torch.jit.trace` only) | ARM64 Toolkit2 supports PyTorch. For QAT models, >= 1.9.0 required. |

Export-side framework version should match the load-side version installed with Toolkit2 — mismatches are a common cause of `load_pytorch` / `load_tensorflow` failures.

> **Version note:** the User Guide §3.1 overview still prints older ranges (e.g. PyTorch 1.6.0–1.13.1, ONNX 1.7.0–1.10.0, TF up to 2.8.0). The API Reference §1.3 table above is the V2.3.2 support statement; use it when the two disagree, and verify on the installed SDK when the user's release differs.

### Older Toolkit2 release ranges

| Toolkit2 | PyTorch | ONNX | TF |
|---|---|---|---|
| 1.4.x – 1.5.2 | 1.6.0 – 1.10.1 | 1.7.0 – 1.10.0 | 1.12.0 – 2.8.0 |
| 1.6.0 | 1.6.0 – 1.13.1 | 1.7.0 – 1.14.0 | 1.12.0 – 2.14.0 |
| 2.0.0 – 2.2.0 | 1.10.1 – 2.1.0 | 1.7.0 – 1.14.1 | 1.12.0 – 2.14.0 |
| **2.3.0 – 2.3.2** | **1.10.1 – 2.4.0** | **1.7.0 – 1.17.0** | **1.12.0 – 2.14.0** |

## rknn_convert YAML wrapper (User Guide §3.1.6)

For batch conversion without writing Python, Toolkit2 ships a CLI wrapper:

```bash
python -m rknn.api.rknn_convert -t rk3588 -i ./model_config.yml -o ./output_path
```

`-t` accepts the same target strings as `config()` (`rv1103`, `rv1103b`, `rv1106`, `rv1106b`, `rv1126b`, `rk2118`, `rk3562`, `rk3566`, `rk3568`, `rk3576`, `rk3588`).

Supported flags:

- `-t` `target_platform` (required)
- `-i` path to YAML config (required)
- `-o` output directory (required)
- `-e` evaluate board perf+memory (requires connected device; pair with `-d` for multi-device)
- `-a "img1.jpg img2.jpg"` accuracy evaluation (simulator; add `-d <device_id>` for board)
- `-v` verbose log
- `-d` / `-d <device_id>` adb device selection

YAML structure (abridged):

```yaml
models:
  name: object_detection
  platform: onnx                 # tensorflow|tflite|caffe|onnx|pytorch|darknet
  model_file_path: ./object_detection.onnx
  subgraphs:
    input_size_list: [1,3,512,512]
    inputs:  [data]
    outputs: [conv6-1, conv6-2, conv6-3]
  quantize: true
  dataset: ./dataset.txt
configs:
  quantized_dtype: asymmetric_quantized-8  # value documented for the YAML wrapper; config() API uses w8a8
  mean_values: [127.5,127.5,127.5]
  std_values:  [128.0,128.0,128.0]
  quant_img_RGB2BGR: false
  quantized_algorithm: normal
  quantized_method: channel
  optimization_level: 3
  single_core_mode: false        # RK3588/RK3576 only
```

Caffe uses `prototxt_file_path` + `caffemodel_file_path`; DarkNet uses `darknet_cfg_path` + `darknet_weights_path`. The wrapper's config table documents fewer keys than `rknn.config()`; knobs such as `auto_hybrid`, `sparse_infer`, `op_target`, `dynamic_input`, and `enable_flash_attention` are not listed there. Pick this wrapper when converting many models with similar shape; use the Python API for anything requiring custom logic or newer knobs.

## Config parameters to handle explicitly

| Parameter | Guidance |
|---|---|
| `target_platform` | Always set the actual SoC. Wrong or missing target is a common cause of invalid model/runtime failures. |
| `mean_values`, `std_values` | Match training/export preprocessing. For multi-input models, use one list per input. For non-image tensors, do not force 3-channel image values. |
| `quant_img_RGB2BGR` | Affects Toolkit2 image loading for quantization/accuracy analysis only. It is ignored for `.npy`; BGR `.npy` data must already be BGR. It is not stored in the RKNN model. |
| `quantized_dtype` | V2.3.2 names: `w8a8` default, `w4a16`, `w8a16`, `w4a8` not yet supported, `w16a16i`, `w16a16i_dfp`. `w8a8` is **not supported on RK2118** — use an FP16 build there. Legacy SDK snippets may show `asymmetric_quantized-8`. |
| `quantized_algorithm` | `normal`, `mmse`, `kl_divergence`, `gdq`. `gdq` is valid only for `w4a16` and `w8a16` and usually needs more than 200 calibration samples. |
| `quantized_method` | `channel` default. `group{SIZE}` such as `group32` or `group128` is valid only with `w4a16`; size is a multiple of 32 between 32 and 256. |
| `optimization_level` | Default 3. Lower to 2/1/0 only to isolate graph optimization conversion or accuracy issues. |
| `dynamic_input` | Use only if the model and target runtime support the intended shape sets. Runtime dynamic queries are not available on RV110x/RK2118. |
| `single_core_mode` | Can reduce model size/memory on RK3588/RK3576 but limits core usage. |
| `op_target` | Force selected ops to a specific execution target. Accepts **tensor-name form** `{'op0_output_name':'cpu'}`, **op_type form** `{'Add':'cpu', 'Expand':'cpu'}`, or **mixed** `{'Add':'cpu', 'op0_output_name':'cpu'}`. Values: `'cpu'`, `'npu'`, `'gpu'`, `'auto'`. Get tensor names from `accuracy_analysis()` output. Default `None`. Document any forced CPU/GPU op — performance can drop. |
| `remove_reshape` | Strip `Reshape` ops from the model's input/output to avoid CPU reshape (slow on most platforms). Default `False`. **Watch for shape changes** — the build log prints warnings; deployment code must handle the new input/output shape. |
| `model_pruning` | Lossless prune of **already-sparse** weights — reduces `.rknn` size and compute. Default `False`. Does not create sparsity; see `references/23_sparsity_and_toolkit_functions.md`. |
| `sparse_infer` | Sparse inference path for a **pre-sparsified** model. **RK3576 / RV1126B only** (Toolkit2 V2.3.2 `config()` table). Default `False`. AutoSparsity **training** docs primarily cover RK3576 — see `references/23_sparsity_and_toolkit_functions.md`. |
| `enable_flash_attention` | Enable FlashAttention (per [arXiv 2307.08691](https://arxiv.org/abs/2307.08691)) — trades larger model size for higher throughput and lower bandwidth on attention-heavy models. Default `False`. See the `exSDPAttention` description in the compiler op list for supported cases. |
| `quantize_weight` | When `do_quantization=False`, quantize some weights anyway to shrink the `.rknn` file. Default `False`. Different from `do_quantization=True` (full INT8 path) — this is a size optimization for FP16 deployment. |
| `float_dtype` | V2.3.2 supports only `'float16'` (default) for the non-quantized (FP) build path. Do not pass `'float32'` unless the installed SDK documents it. Only meaningful when `do_quantization=False`. |
| `remove_weight` | `False` (default). When `True`, the exported `.rknn` omits conv-class weights — used to build a "slave" model that shares weights with a master at runtime via `RKNN_FLAG_SHARE_WEIGHT_MEM`. See `references/13_advanced_features.md`. |
| `compress_weight` | `False` (default). Compresses weights in the exported model. Verify the target runtime supports compressed weights before enabling. |
| `inputs_yuv_fmt` | Listed in the Toolkit1→2 API diff as a new `config()` argument, but V2.3.2 docs do not document its accepted values. Do not assume NV12/NV16 semantics — check the installed SDK release notes/headers before relying on it. |
| `custom_string` | Arbitrary string baked into the `.rknn`; read back at runtime via `RKNN_QUERY_CUSTOM_STRING`. Use for version tagging, model identification, or build metadata. |
| `auto_hybrid_cos_thresh` / `auto_hybrid_euc_thresh` | Thresholds used by `build(auto_hybrid=True)`: cosine default `0.98`; euclidean default `None` (disabled). Tune only after measuring accuracy. |

## Build choices

```python
rknn.build(do_quantization=True, dataset='./dataset.txt')   # quantized output
rknn.build(do_quantization=False)                           # FP16/non-quantized output
rknn.build(do_quantization=True, dataset='./dataset.txt', auto_hybrid=True)
```

`auto_hybrid=True` converts low-accuracy ops to FP16 during a **w8a8** quantized build (the only quantized mode it supports), or overflow-risk ops to INT16 during a non-quantized build. `w4a16` / `w8a16` builds need manual `hybrid_quantization_step1/2` instead. Tune thresholds with `auto_hybrid_cos_thresh` and `auto_hybrid_euc_thresh` in `config()` only after measuring accuracy.

When `rknn_batch_size` is used, input/output shapes change. Update input allocation, C API postprocess, and result splitting.

## Multi-input and data format

- Dataset rows for multiple inputs separate files with spaces, for example `a.jpg b.npy`.
- Set `mean_values` and `std_values` per input.
- Pass `data_format` per input in `inference()`, such as `['nhwc', 'nchw', 'nchw']`.

## Connected inference/evaluation

```python
ret = rknn.init_runtime(target='rk3588', device_id=None, perf_debug=True, eval_mem=True)
outputs = rknn.inference(inputs=[input_data], data_format=['nhwc'])
rknn.eval_perf(is_print=True, fix_freq=True)
rknn.eval_memory(is_print=True)
```

### `init_runtime()` parameter reference (Toolkit2 API Ref §2.7)

| Parameter | Default | Notes |
|---|---|---|
| `target` | `None` (simulator) | One of the supported SoC strings **except `rk2118`** (no connected-runtime path). `None` runs on the PC simulator — requires `build()` or `hybrid_quantization_step2()` to have been called first. |
| `device_id` | `None` | Use `list_devices()` to find it. For network-adb devices, `adb connect <IP>` first, then pass `[IP]` or `[IP:Port]`. |
| `perf_debug` | `False` | `True` returns per-layer timing from `eval_perf()`; `False` returns only total time. **Not supported on RV1103/RV1103B/RV1106/RV1106B.** |
| `eval_mem` | `False` | `True` enables `eval_memory()`. |
| `core_mask` | `RKNN.NPU_CORE_AUTO` | RK3588 / RK3576 only. RK3576 has 2 cores — do NOT use `NPU_CORE_2` or `NPU_CORE_0_1_2`. |
| `async_mode` | `False` | Overlaps next-frame input set with current-frame inference. **Currently unsupported** (placeholder for future releases); first frame is always sync. |
| `fallback_prior_device` | `'cpu'` | When an op exceeds NPU spec, fall back to `'cpu'` or `'gpu'`. `'gpu'` only valid on SoCs with a GPU. |

### `inference()` parameter reference (Toolkit2 API Ref §2.8)

- `inputs` — list of `ndarray`, one per model input.
- `data_format` — per-input layout `'nchw'` / `'nhwc'`, 4-D inputs only. Default `None` = all NHWC.
- `inputs_pass_through` — per-input list of `0`/`1`. `1` skips mean/std normalization and passes the raw buffer to the NPU. Use `[1, 0]` to pass input 0 raw but normalize input 1. Default `None` = normalize all.

### `load_rknn()` constraints (Toolkit2 API Ref §2.6)

Loading a pre-built `.rknn` via `rknn.load_rknn(path)` **skips** `config`/`load_*`/`build`. After `load_rknn`, the model can ONLY be used for:

- Connected-NPU inference (`init_runtime(target=...)` + `inference()`)
- Performance / memory evaluation (`eval_perf`, `eval_memory`)
- SDK version query (`get_sdk_version`)

It **cannot** be used for simulator inference or `accuracy_analysis()` — those need the original framework model loaded through `load_onnx`/`load_pytorch`/etc. If a user needs accuracy_analysis on a `.rknn` they did not build, ask for the source model.

### `get_sdk_version()` returns three versions (Toolkit2 API Ref §2.11)

```python
>>> print(rknn.get_sdk_version())
RKNN VERSION:
API: 2.3.2 (abc1234 build@2025-04-03T...)
DRV: rknn_server: 2.3.2 (abc1234 build@2025-04-03T...)
DRV: rknnrt: 2.3.2 (def5678@2025-04-03T...)
```

All three should match for a healthy deployment: **API** = Toolkit2 Python lib, **rknn_server** = board-side proxy, **rknnrt** = `librknnrt.so`/`librknnmrt.so`. A mismatch here is the most common cause of `RKNN_ERR_MODEL_INVALID` and "abnormal server" errors.

### `list_devices()` returns two lists (Toolkit2 API Ref §2.14)

```python
adb_devices, ntb_devices = rknn.list_devices()
```

Devices can connect in **ADB mode** (standard) or **NTB mode** (network transfer box). For multi-device workflows, all devices must use the same mode — mixing causes connection failures.

### Auto-generate C++ deployment scaffold: `codegen()` (Toolkit2 API Ref §2.17)

```python
rknn.codegen(
    output_path='./rknn_app_demo',
    inputs=['./dog_224x224.jpg'],   # optional; jpg/png/npy, npy shape must match input
    overwrite=True,                  # default False
)
```

Produces a working C++ demo for the just-built model — useful as a starting point instead of writing from scratch or copying a Model Zoo demo. Pair with `assets/c_api_*.c` templates in this skill when you need to customize beyond what `codegen` emits.

Use `load_rknn()` for an already built RKNN model. Use `accuracy_analysis()` before board performance work when quantized output is wrong.
