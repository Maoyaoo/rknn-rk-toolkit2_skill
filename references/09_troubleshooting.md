# RKNN Troubleshooting Playbook

Use this file after identifying target SoC, SDK version, runtime mode, and whether the failure occurs during conversion, simulator inference, connected inference, or board runtime.

## Symptom → section map

| Symptom | Jump to |
|---|---|
| `RKNN_ERR_DEVICE_UNAVAILABLE` / rknn_server abnormal / `adb devices` empty | [`rknn_init` fails: device unavailable](#rknn_init--init_runtime-fails-device-unavailable) |
| `Invalid RKNN model version` / `Invalid RKNN format` / model fails to load on board | [Invalid model or RKNN model version errors](#invalid-model-or-rknn-model-version-errors) |
| Quantized output is garbage but FP16 simulator output is fine | [Quantized output completely wrong](#quantized-output-completely-wrong) |
| Demo worked, my model doesn't | [Model Zoo result is wrong after replacing the model](#model-zoo-result-is-wrong-after-replacing-the-model) |
| Mean/std or input shape mismatch warnings | [Mean/std or input shape errors](#meanstd-or-input-shape-errors) |
| RV110x: `failed to allocate fd`, OOM, `RKNN_ERR_MODEL_INVALID` on fresh board | [RV1103/RV1106 memory or fd allocation errors](#rv1103rv1106-memory-or-fd-allocation-errors) + [rkipc kill](#rv110x-rknn_err_model_invalid-at-init-on-a-fresh-board) |
| RV110x B-variant: `.rknn` built for `rv1106` fails on `rv1106b` | [RV110x B-variant model mismatch](#rv110x-b-variant-model-mismatch) |
| `rknn.build()` fails with shape/dtype/_p_fuse errors | [Common conversion errors](#common-conversion-errors-user-guide-106) |
| Quantization runs but process gets SIGKILL'd | [Conversion-stage quantization OOM](#conversion-stage-quantization-oom-user-guide-105) |
| RK2118: `adb devices` empty | [RK2118: adb cannot see the board](#rk2118-adb-cannot-see-the-board) |
| Perf / MAP lower than Model Zoo reference | [Model Zoo FAQ highlights](#model-zoo-faq-highlights) |
| YOLO confidences > 1, boxes fill image, wrong box sizes | [YOLO-specific symptoms](#yolo-specific-symptoms-quick-decode) |

For RKNN C API return codes (`RKNN_ERR_*`), see the [error code table in `references/04_board_debug_rknn_server.md`](04_board_debug_rknn_server.md#rknn-error-codes-rknnrt-api-ref-5).

## `rknn_init` / `init_runtime` fails: device unavailable

Checklist:

- `adb devices` or `rknn.list_devices()` sees the board.
- `rknn_server` is running for connected inference.
- Toolkit2, `rknn_server`, runtime library, driver/BSP, and RKNN model target match.
- `target` in `init_runtime()` matches the board.
- `target_platform` used during build matches the board.
- Docker does not have a conflicting `npu_transfer_proxy`.
- Debian board `adbd` listens on port 5037 when connected inference reports abnormal server.

### `adb devices` is empty

Quick Start §6.1 triage order:

1. Re-seat the USB cable, try another port, try another cable. On Rockchip boards the data port is labelled **OTG** (Linux/Android) — make sure you are not plugged into a power-only port.
2. `command 'adb' not found` → install it: `sudo apt install adb`.
3. **Only one `adb` server across host and Docker.** If you are running Toolkit2 inside a Docker container with `-v /dev/bus/usb:/dev/bus/usb`, the host's `adb server` will own the device and the container will see nothing. Kill the host server first: `adb kill-server` on the host, then retry inside the container. Symptom: `adb devices` shows the board on the host but is empty in the container (or vice versa).
4. Debian firmware may have `adbd` not listening on port 5037 — see the dedicated section below.

## Invalid model or RKNN model version errors

Likely causes:

- Toolkit2 generated a model newer than board Runtime.
- Runtime and `rknn_server` versions are mismatched.
- Wrong `target_platform`.
- Incomplete or stale `.rknn` file copied to board.

Fix by updating Toolkit2/Runtime/server/firmware together, rebuilding with explicit target, and logging `RKNN_QUERY_SDK_VERSION`.

## Quantized output completely wrong

Compare in this order:

1. Original framework output.
2. FP16 RKNN simulator output.
3. Quantized RKNN simulator output.
4. Connected or board runtime output.

Then check RGB/BGR, layout, mean/std, dtype, `quant_img_RGB2BGR`, calibration data, `want_float`, output dequantization, and postprocess assumptions.

## Model Zoo result is wrong after replacing the model

- Follow the demo export README exactly.
- Verify postprocess matches model structure.
- YOLO: missing sigmoid can make confidences exceed 1 or fill the whole image with boxes.
- YOLOv5/YOLOv7: wrong anchors can produce correct confidence but wrong box sizes.
- Fixed-shape RKNN YOLO can have lower MAP than official dynamic-shape evaluation.
- C demo image reader differences (`cv2`, `stbi`, `turbojpeg`) can affect MAP.

## Mean/std or input shape errors

If model input is not a 3-channel image, do not set `[R,G,B]` mean/std. Match actual channel count and tensor shape, or omit mean/std when preprocessing is outside the model.

For multi-input models, provide one mean/std entry per input and one `data_format` entry per runtime input.

## RV1103/RV1106 memory or fd allocation errors

Use smaller model/resolution, quantize, reduce outputs, stop extra board apps (`RkLunch-stop.sh` stops the non-essential set in one step), use correct `librknnmrt.so`, remove stale duplicate libraries, and upgrade `rknn_server`/Runtime. If `rkipc` is running, stop it explicitly — see `references/06_performance_memory.md`.

For connected `accuracy_analysis()` storage failures, set `RKNN_DUMP_DIR` to `/userdata/dumps` or an NFS-backed path.

## RV110x: RKNN_ERR_MODEL_INVALID at init on a fresh board

Symptom: `rknn_init` or Toolkit2 `init_runtime` returns `RKNN_ERR_MODEL_INVALID` even though the model builds and runs elsewhere. Cause on RV1103/RV1106/RV1103B/RV1106B: the board's `rkipc` (camera/ISP) process is consuming the scarce RAM. Fix:

```bash
adb shell 'ps | grep rkipc'
adb shell 'kill -9 <pid>'          # or killall rkipc
```

See `references/06_performance_memory.md` for the full low-memory checklist.

## RV110x B-variant model mismatch

Symptom: a `.rknn` built with `target_platform='rv1106'` fails to load on an `rv1106b` board (or vice versa), typically with a model-version or invalid-model error. Cause: **models built for `rv1103`/`rv1106` are NOT compatible with `rv1103b`/`rv1106b`** — each variant needs its own build with the matching `target_platform`. Re-export from Toolkit2 with the correct target. The C/C++ demo code itself IS shared across B/non-B variants; only the `.rknn` differs. See `references/01_platform_matrix.md`.

## Common conversion errors (User Guide §10.6)

A quick triage for the most common `rknn.build()` failures. Most are model-graph issues, not Toolkit2 bugs.

| Error signature | Cause | Fix |
|---|---|---|
| `IndexError: vector::_M_range_check: _n (which is 4) >= this->size() (which is 4)` | 5-D or 6-D tensor in the graph (RKNN caps at 4-D) | Reshape > 4-D tensors to 4-D before the op; or move the offending op to CPU postprocess |
| `Meet unsupported dims in reducesum, dims: 6` (or similar) | same — > 4-D op | same |
| `The Op of 'NonZero' is not support! it will cause the graph to be a dynamic graph!` | Op produces data-dependent shapes | Rewrite the graph to remove the op; move to CPU postprocess |
| `NonMaxSuppression` / GridSample / Loop / Scan / Einsum rejected | unsupported postprocess op | Clip the model earlier with `load_onnx(outputs=[...])` and run the rest on CPU |
| `KeyError: 'aten::softmax'` (or `aten::xxx`) | PyTorch op not supported by Toolkit2 version | Upgrade Toolkit2 to latest; if still unsupported, replace op in model |
| `[enforce fail at inline container.cc:137]. PytorchStreamReader failed reading zip archive` | Tried to load `.pth` (weights only, no graph) | Re-export via `torch.jit.trace()` → `.pt` |
| `'torch._C' has no attribute '_jit_pass_inline'` | PyTorch < 1.6.0 | Upgrade PyTorch to >= 1.6.0 |
| `Not support input data type 'float16'` | PyTorch model with FP16 weights | Re-export with FP32 weights |
| `Error parsing message` (protobuf) | ONNX file truncated/corrupted | Re-download; verify MD5 |
| `invalid expand shape` (onnxruntime, during `fold_constant`) | Model uses runtime-input values to compute shape (`downsample_ratio`, etc.) | Rewrite so input values don't control graph shape; use `dynamic_input` instead |
| `E load_onnx: the '378' in outputs=['378',...] is invalid!` | Wrong output node name in `load_onnx(outputs=...)` | Open the model in Netron, copy exact output node names |
| `Shape must be rank 4 but is rank 0` (TF multi-input) | `input_size_list` shape wrong for multi-input | One shape per input: `[[1,128,128,3], [1,128,128,3], ...]` |
| `The len of mean_values ([128,128,128]) for input 0 is wrong, expect 32!` | Model input isn't 3-channel RGB but mean/std is set as if it were | Match mean/std length to actual input channel count, or omit mean/std if input isn't an image |
| TF: `Node 'X' expects to be colocated with unknown node 'Y'` | TF 1.x model | Install TF 2.x and re-export |
| `infer_shapes` / `_p_fuse_two_mul` / `_p_fuse_mul_into_conv` ValueError | Toolkit2 graph-optimizer bug | Upgrade Toolkit2 to latest |

If the error persists after the fix and a Toolkit2 upgrade, capture `verbose=True` logs and a minimal reproducer, and report to the Rockchip NPU team.

## Conversion-stage quantization OOM (User Guide §10.5)

`rknn.build(do_quantization=True, ...)` allocates a lot of host memory — on a low-RAM dev machine or a small container, the process gets SIGKILLed or hangs. Add swap (e.g. a 16 GB swap file on Linux). The `mmse` quantization algorithm is particularly memory-hungry.

## RK2118: adb cannot see the board

Symptom: `adb devices` is empty for an RK2118 board. Cause: RK2118 does NOT use the adb transport — it is an RT-Thread SoC, not Linux/Android. Connect via the **SYSTEM DEBUG** serial port (baud **1500000 8N1**, e.g. `minicom -D /dev/ttyUSB0`). There is no `rknn_server` connected-debug path either; RK2118 runs models locally via its RT-Thread NPU SDK. See `references/14_rk2118_rtos_deploy.md`.

## Model Zoo FAQ highlights

Distilled from the RKNN Model Zoo FAQ (V2.3.2 era). Apply after the basic triage above. For the original wording or build scripts, use the `rknn_model_zoo` release that matches the user's SDK.

### Result differs across platforms

Small numeric differences across NPU generations are normal. Significant differences are not — submit an issue after confirming versions align.

### Performance lower than Model Zoo reference

- Model Zoo benchmark times ONLY `rknn_run`, no pre/post-processing. Don't compare end-to-end app time against the published number.
- Python RKNN API is slower than C — measure with the C API.
- Fix CPU/NPU/DDR frequency (`scaling_frequency.sh`) before comparing.
- On big.LITTLE CPUs (RK3588, RK3576), bind the test to big cores.
- Stop other CPU/NPU/bandwidth consumers on the board.

### OpenCV and libjpeg_turbo segfault

OpenCV's bundled libjpeg conflicts with `rknn_model_zoo/3rdparty/jpeg_turbo`. Fix:

1. Recompile OpenCV without JPEG support, OR
2. Pass `-j` to `build-linux.sh` / `build-android.sh` to disable jpeg_turbo (use OpenCV's JPEG path).

### Build fails / missing dependent libraries

Cause: the cross-toolchain on the host differs from the one Model Zoo was built against. Follow the exact toolchain guidance from the Model Zoo build docs or the Quick Start notes summarized in `references/10_model_zoo_workflows.md`, then **delete the old `build/` directory** before rebuilding.

### Demo ran correctly, but my own model runs wrong

Almost always the model export didn't follow the demo README. Use the Model Zoo-recommended fork (e.g. `airockchip/yolov5`) and its RKNN-specific export branch; keeping the original graph will diverge on postprocess, accuracy, and performance.

### Why RV1103/RV1106 have fewer demos

Memory size limit excludes larger models. Don't read it as "unsupported op" — port smaller variants and check actual memory headroom before deployment.

### "No demo for my model — is it unsupported?"

**No.** Model Zoo demos are a curated usefulness set, not an allow-list of supported networks. Absence of a demo is not an operator or SoC refusal. Validate with Toolkit2 conversion + `references/07_operator_support.md` / `rknn_doc` op lists, and closest-family demos for pre/postprocess patterns only.

### "Is there a large-model / LLM demo?"

**Not in the V2.3.2-era Model Zoo FAQ.** Transformer support is evolving; do not invent a zoo LLM demo path. For attention-heavy models, check `enable_flash_attention` / compiler `exSDPAttention` notes in `references/02_toolkit2_conversion.md` and op lists — still not a substitute for a full LLM product demo.

### Board-side Python exists (Lite2)

Yes for RKNPU2: install Toolkit-Lite2 on board, swap `from rknn.api import RKNN` → `from rknnlite.api import RKNNLite as RKNN` on zoo Python scripts, or use `assets/lite2_python_infer.py`. Details: `references/19_toolkit_lite2_board_python.md`. Prefer C for performance-sensitive deploy.

### RGA issues

Refer to the bundled RGA raw docs in `rknn_doc/Rockchip_FAQ_RGA_CN.md` and `rknn_doc/Rockchip_Developer_Guide_RGA_CN.md` when you need low-level RGA behavior. Common: stale `librga.so` vs kernel driver. In the FAQ table, `librga` >1.4.0 requires the RGA multicore driver >=1.2.0; `librga` >1.9.0 also works with the listed RGA/RGA2 drivers. Keep the pair matched.

## YOLO-specific symptoms (quick decode)

- **Confidence > 1 / boxes fill the image**: model is missing the trailing sigmoid. Re-export from the recommended fork with the sigmoid branch.
- **Box position/confidence OK but wrong size**: anchors mismatch between export and demo config (YOLOv5/YOLOv7).
- **MAP lower than official**: official uses dynamic shape, RKNN port uses fixed shape; quantization also costs some MAP. C-side image reader (`cv2` vs `stbi` vs `turbojpeg`) shifts MAP slightly too.
- **Can NPU run YOLO without modifying the structure?** Technically yes, but not recommended: unmodified graphs usually quantize worse and run slower.

See `references/03_quantization_accuracy.md` for the full accuracy triage sequence and `references/10_model_zoo_workflows.md` for porting rules.
