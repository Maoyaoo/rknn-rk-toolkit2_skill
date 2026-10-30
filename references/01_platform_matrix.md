# RKNN Platform Matrix

Use this file before selecting conversion targets, Runtime APIs, runtime libraries, or Model Zoo build options.

## RKNPU1 vs RKNPU2 — do not mix

Two incompatible Rockchip NPU software stacks exist:

| Stack | SDK / Toolkit | Runtime library | Typical SoCs |
|---|---|---|---|
| RKNPU1 (legacy) | RKNN-Toolkit (v1.x) | `librknn_runtime.so` | RK1808, RV1109, RV1126, RK3399Pro |
| RKNPU2 (current) | RKNN-Toolkit2 (V2.x) | `librknnrt.so` / `librknnmrt.so` | RK3562/RK3566/RK3568/RK3576/RK3588/RV1103/RV1103B/RV1106/RV1106B/RV1126B/RK2118 |

This skill targets **RKNPU2 only**. Model Zoo keeps separate `rknpu1/` and `rknpu2/` demo trees; do not port a `rknpu1/` demo expecting it to work with Toolkit2 or `librknnrt.so`. RKNPU1 APIs (`rknn_register_op`, old runtime flags) are not compatible with RKNPU2 runtime.

If a user references RK1808/RV1109/RV1126 (non-B)/RK3399Pro or Toolkit v1, state that this skill does not cover RKNPU1 and point them to RKNN-Toolkit v1 documentation; do not apply the V2.3.2 rules below. Note that `rv1126` (RKNPU1) and `rv1126b` (RKNPU2) are different platforms.

## Toolkit2 target strings

Use lowercase `target_platform` values in RKNN-Toolkit2 scripts:

`rv1103`, `rv1103b`, `rv1106`, `rv1106b`, `rv1126b`, `rk2118`, `rk3562`, `rk3566`, `rk3568`, `rk3576`, `rk3588`.

`rk3588s` is **not** a separate target string in V2.3.2 — use `rk3588`, which covers both RK3588 and RK3588S.

### RV110x B-variant model incompatibility

**Models built for `rv1103`/`rv1106` are NOT interchangeable with models built for `rv1103b`/`rv1106b`.** Each SoC variant needs its own `.rknn` build with the matching `target_platform`. (Source: RV1106/RV1103 Quick Start §4.2.2.) The non-model code — C/C++ demos, build scripts — IS shared between the B and non-B variants, only the `.rknn` file differs. Set `target_platform` explicitly; do not reuse a model across these variants.

### Cross-platform model compatibility (User Guide §10.3)

Within these groups, a single `.rknn` runs on any member SoC without rebuild:

| Group | Compatible `target_platform` values |
|---|---|
| RK3566 / RK3568 | either `rk3566` or `rk3568` produces a model that runs on both |
| RK3588 / RK3588S | build with `rk3588`; it runs on both |
| RV1103 / RV1106 | either `rv1103` or `rv1106` runs on both (non-B variants only) |
| RV1103B / RV1106B | either `rv1103b` or `rv1106b` runs on both (B variants only) |

The API default for `target_platform` is `None`, but a model built without it is only guaranteed to run on RK3566/RK3568 (Toolkit2 documents this as the RK3566-class default). For any other SoC, set `target_platform` explicitly.

## Runtime library and deployment family

| Target family | Runtime library | Deployment note |
|---|---|---|
| RK3562/RK3566/RK3568/RK3576/RK3588/RV1126B | `librknnrt.so` | Normal RKNPU2 runtime layout. |
| RV1103/RV1103B/RV1106/RV1106B | `librknnmrt.so` | Use `armhf-uclibc` runtime/server paths and zero-copy style demos. |
| RK2118 | RK2118 mini runtime (librknnmrt-style) | Same mini-runtime API family as RV1103/RV1106 (`rknn_set_io_mem`, zero-copy; no `rknn_inputs_set`/`rknn_outputs_get`). Library ships at `rt-thread/components/rknpu/lib/` in the RT-Thread SDK — NOT from `rknpu2/runtime`. See `references/14_rk2118_rtos_deploy.md`. |

Only **RK3588 (3 cores)** and **RK3576 (2 cores)** expose multi-core control (`rknn_set_core_mask`, `rknn_set_batch_core_num`). Treat every other RKNPU2 SoC as single-core when planning performance.

## Host-side Toolkit2 differences

| Host Toolkit2 | Conversion support | Connected debugging |
|---|---|---|
| x86 Linux | Caffe, TensorFlow, TFLite, ONNX, DarkNet, PyTorch | Uses adb + board-side `rknn_server`. |
| ARM64 Linux | ONNX and PyTorch only | Runs locally on board; no adb/rknn_server connected debug path; no Docker install. |
| Docker x86 | Same as x86 image | Pass USB devices through and avoid conflicting host `npu_transfer_proxy`. |
| WSL2 | Conversion works after Linux dependencies | Prefer Ethernet adb; USB needs a Windows bridge such as adbkit. |

## Detecting the board's OS and architecture

Before pushing `librknnrt.so` or choosing `build-android.sh` vs `build-linux.sh`, identify what the board actually runs (Quick Start §3.3.2.1):

```bash
# Android vs Linux: a numeric reply means Android; "getprop: not_found" means Linux
adb shell getprop ro.build.version.release

# Architecture on Android
adb shell getprop ro.product.cpu.abi        # -> arm64-v8a or armeabi-v7a

# Architecture on Linux
adb shell uname -a                          # -> ... aarch64 GNU/Linux  (or armv7l)
```

Map results to the runtime path:

| Board OS | `ro.product.cpu.abi` / `uname -m` | Runtime lib path | Build script |
|---|---|---|---|
| Android 64-bit | `arm64-v8a` | `/vendor/lib64/librknnrt.so` | `build-android.sh -a arm64-v8a` |
| Android 32-bit | `armeabi-v7a` | `/vendor/lib/librknnrt.so` | `build-android.sh -a armeabi-v7a` |
| Linux 64-bit | `aarch64` | `/usr/lib64/librknnrt.so` (some firmware: `/usr/lib/`) | `build-linux.sh -a aarch64` |
| Linux 32-bit | `armhf` / `armv7l` | `/usr/lib/librknnrt.so` | `build-linux.sh -a armhf` |

RV1103/RV1106 (and B variants) are the exception to this table: runtime is `/oem/usr/lib/librknnmrt.so`, arch is `armhf-uclibc`, and the toolchain is `arm-rockchip830-linux-uclibcgnueabihf` (see `references/04_board_debug_rknn_server.md`, `references/10_model_zoo_workflows.md`).

## C API support reminders

- `rknn_init`, `rknn_destroy`, `rknn_query`, `rknn_run`, memory creation/destruction, `rknn_set_weight_mem`, `rknn_set_internal_mem`, `rknn_set_io_mem`, and `rknn_mem_sync` are broadly available.
- `rknn_inputs_set`, `rknn_outputs_get`, and `rknn_outputs_release` are not supported on RV1103/RV1106/RV1103B/RV1106B/RK2118 in the V2.3.2 Runtime API table.
- `rknn_set_input_shapes`, `RKNN_QUERY_INPUT_DYNAMIC_RANGE`, and current dynamic attr queries are not supported on RV1103/RV1106/RV1103B/RV1106B/RK2118.
- `rknn_set_core_mask` and `rknn_set_batch_core_num` are for RK3588/RK3576. RK3576 has two cores; do not select core 2 or core mask 0_1_2.
- Matmul APIs and custom op APIs are not universal. Check the Runtime API table before generating them.

## Model Zoo build target aliases

Model Zoo build scripts normalize both group aliases and exact SoC names:

| Board/SoC | Model Zoo `-t` value |
|---|---|
| RK3562/RK3566/RK3568 | `rk356x` (direct `rk3562` / `rk3566` / `rk3568` also accepted and normalized to `rk356x`) |
| RK3576 | `rk3576` |
| RK3588 | `rk3588` |
| RV1126B | `rv1126b` |
| RV1103/RV1106 | `rv1106` or `rv1103` |

`rv1126` (without `b`) is the legacy RV1126/RV1109 target and belongs to RKNPU1 — do not confuse it with `rv1126b`.

For RV1103/RV1106 Linux demos, export `GCC_COMPILER` to the Rockchip uClibc toolchain prefix, for example `arm-rockchip830-linux-uclibcgnueabihf`.
