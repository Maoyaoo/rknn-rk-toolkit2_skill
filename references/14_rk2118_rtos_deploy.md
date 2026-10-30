# RK2118 RT-Thread Deployment

RK2118 is the outlier in the RKNPU2 family: it does NOT run Linux or Android, so most of the generic RKNN workflow (adb push, `rknn_server` connected debugging, `/usr/lib/librknnrt.so`) does not apply. Source: `rknn_doc/MinerU_markdown_01_Rockchip_RK2118_Quick_Start_*.md`.

Use this file when the target is `rk2118`. For RV110x (`rv1103`/`rv1106`/`rv1103b`/`rv1106b`) see the regular Linux/uClibc path in `references/04_board_debug_rknn_server.md` and `references/06_performance_memory.md`.

## What's different about RK2118

| Concern | Linux/Android RKNPU2 | RK2118 |
|---|---|---|
| OS | Linux / Android | RT-Thread (RTOS) |
| Host requirement | any | Ubuntu 18.04 / 20.04 / 22.04 |
| Board connection | `adb` over USB/TCP | **Serial** on SYSTEM DEBUG port, 1500000 8N1 — adb does NOT see the board |
| Firmware build | kernel + rootfs | RT-Thread firmware image (`Image/Firmware.img`) built via `scons` + `mkimage.sh` |
| Runtime library | `librknnrt.so` or `librknnmrt.so` | **Mini runtime, same API family as RV1103/RV1106** — sourced from `rt-thread/components/rknpu/lib/`, NOT the regular `rknpu2/runtime` layout |
| Connected debugging | PC Toolkit2 ↔ `rknn_server` on board | None — RK2118's `rknn_server` is a DSP/MCU interaction service, NOT a Toolkit2 connected-debug proxy |
| Model deployment | `adb push model.rknn ...` | FAT32 USB stick (`USB 2.0 HOST` port) OR bake into firmware resource partition |

## Mini runtime interface — important

The RK2118 RKNN SDK uses **the same mini runtime interface as RV1103/RV1106** (quoted: "RK2118 RKNN SDK使用与RV1103/RV1106 SDK类似的mini runtime接口"). Concretely:

- C demos follow the **RV110x zero-copy style** in `references/05_runtime_c_api.md` — `rknn_set_io_mem` with native attrs, `UINT8` input, NHWC.
- Do NOT generate `rknn_inputs_set` / `rknn_outputs_get` code for RK2118 — those general-API calls are unsupported, same as on RV110x.
- Dynamic shape, `RKNN_QUERY_INPUT_DYNAMIC_RANGE`, matmul API, and most custom-op paths are unavailable — see `references/11_dynamic_shape_and_mem_reuse.md` and `references/05_runtime_c_api.md`.

## Target string

Toolkit2 uses `target_platform='rk2118'` for conversion. The conversion flow itself is unchanged — `RKNN()` → `config(target_platform='rk2118', ...)` → `load_*` → `build` → `export_rknn`. The `.rknn` artifact is deployed through the RK2118 system SDK, not through `librknnrt.so`.

## Board connection

- Use a USB-to-TTL adapter or the on-board USB-C port labelled **SYSTEM DEBUG**.
- Serial parameters: **1500000 baud, 8 data bits, no parity, 1 stop bit** (1500000 8N1).
- Tool example: `minicom -D /dev/ttyUSB0 -b 1500000` or `picocom -b 1500000 /dev/ttyUSB0`.
- The `USB2.0 HOST` port is for flashing / USB storage, not debug — don't confuse it with the SYSTEM DEBUG port.
- If `adb devices` is empty for an RK2118 board, that is expected (Quick Start §6.1) — switch to serial.

## SDK layout

The RKNPU code lives inside the RT-Thread SDK at `rt-thread/components/rknpu/`:

```
rt-thread/components/rknpu/
├── examples/      # rknn_benchmark, mobilenet, etc.
├── include/       # rknn_api.h (mini runtime variant)
├── lib/           # RK2118 runtime library
├── rknn_server/   # DSP/MCU interaction service — NOT Toolkit2 connected debug
└── SConscript
```

The canonical first example is `examples/rknn_benchmark/`:

```
rknn_benchmark/
├── README.md
├── res/
│   └── convert.py        # model → .rknn conversion script
├── SConscript
├── src/
│   └── rknn_benchmark.c  # benchmark entry point
└── tools/
    └── update_res.py     # generates the .bin input file from sample data
```

## Firmware build workflow

### Get the SDK

The RT-Thread platform SDK is fetched with Rockchip's `repo` tool. Access requires permission from Rockchip — apply through your Rockchip contact.

```bash
# repo tool (skip if already installed)
git clone ssh://git@www.rockchip.com.cn/repo/rk/tools/repo -b stable
export PATH=/path/to/repo:$PATH

# SDK checkout
mkdir rt-thread && cd rt-thread
repo init --repo-url ssh://git@www.rockchip.com.cn/repo/rk/tools/repo \
    -u ssh://git@www.rockchip.com.cn/rtos/rt-thread/rk/platform/release/manifests \
    -b master -m rk2118.xml
.repo/repo/repo sync
```

Toolchain setup follows `rt-thread/RKDocs/manuals/rk2118/Rockchip_Developer_Guide_RK2118.pdf`.

### Enable RKNPU in menuconfig

```bash
cd bsp/rockchip/rk2118
cp board/evb/defconfig .config
scons --menuconfig
```

Two option blocks must be on. The DFS/file-system block is required because models load from FAT32 USB:

```
RT-Thread Components --->
[*] DFS: device virtual file system --->
    [*] Using posix-like functions, open/read/write/close
    [*] Using working directory
    (4) The maximal number of mounted file system
    (4) The maximal number of file system type
    (16) The maximal number of opened files
    [*] Using mount table for file system
    [*] Enable elm-chan fatfs

RT-Thread rockchip RK2118 drivers --->
[*] Enable RKNPU
```

### Build and pack

```bash
cd bsp/rockchip/rk2118
scons -j32
./mkimage.sh board/evb/setting.ini
```

Output: `Image/Firmware.img`. Flash per `Rockchip_Developer_Guide_RK2118.pdf` §4.1.3.4.

**The default `defconfig` build already includes `rknn_benchmark`** — no separate demo compile is needed for a first run.

## Model conversion example

The Quick Start's worked example converts **TensorFlow `.pb`** (MobileNetV1), not ONNX. From `rt-thread/components/rknpu/examples/rknn_benchmark/res/`:

```bash
cd rt-thread/components/rknpu/examples/rknn_benchmark/res
python convert.py
```

Produces:

- `mobilenetv1.rknn` — the deployed model.
- `dog.bin` — pre-baked model input. Generated by `tools/update_res.py` from a sample image; the runtime consumes this binary blob directly.

For ONNX/PyTorch sources, use the same Toolkit2 conversion flow with `target_platform='rk2118'`, but the canonical demo ships TensorFlow.

## Deploy and run

1. Copy `mobilenetv1.rknn` and `dog.bin` to the root of a **FAT32-formatted USB stick**.
2. Plug the stick into the board's **`USB 2.0 HOST`** port.
3. From the serial console:

```
cd /
ls                       # should show the model + input files
rknn_benchmark mobilenetv1.rknn dog.bin
```

Usage: `rknn_benchmark <model_path> <input_path>`. Output prints TOP-5 class IDs and scores; **no softmax** is applied by default.

For production, bake the model into the firmware image instead of using a USB stick.

## rknn_server on RK2118 — what it actually is

A `rknn_server` binary does exist under `rt-thread/components/rknpu/rknn_server/`, but its purpose on RK2118 is **DSP ↔ MCU interaction**, NOT the PC Toolkit2 connected-debug proxy used on Linux/Android (Quick Start §6.2). Consequences:

- Toolkit2's `init_runtime(target='rk2118')` connected-debug path does not work.
- Validate models on the PC simulator first; deploy the `.rknn` to the board and run locally.
- Do not try to `adb push` `librknnrt.so` or restart `rknn_server` for Toolkit2 debugging — it isn't that kind of service here.

## Toolkit2 host install (brief)

- Python **3.8** recommended; `miniforge` if you need to manage versions.
- Install via pip: `pip install rknn-toolkit2 -i https://pypi.org/simple`.
- Or via the local wheel under `rknn-toolkit2/rknn-toolkit2/packages/x86_64/` (pick the matching `cpxx` and manylinux tag).
- Verify: `python -c "from rknn.api import RKNN"`.
- A Docker image is also published for running the conversion without polluting the host (see Quick Start ch. 5).

## What stays the same as the rest of RKNPU2

- Toolkit2 conversion API and config knobs (mean/std, quantized_dtype, quantized_algorithm, etc.).
- The `.rknn` model format and quantization modes supported on RK2118 — verify per-op support against the compiler support PDF for the `rk2118` column, since RK2118 lacks several fused ops present on RK3588/RK3576 (see `references/07_operator_support.md`).
- Accuracy triage flow (`references/03_quantization_accuracy.md`) — but on the PC simulator only; there is no board-side connected-debug path.

## Common pitfalls

- Trying `adb push` / `adb shell` against an RK2118 board — silent or "device offline". Use serial.
- Copying `librknnrt.so` from a regular SDK tree to the board. Use the RK2118 mini runtime under `rt-thread/components/rknpu/lib/`.
- Expecting `rknn_server` for connected debugging. RK2118's `rknn_server` is a DSP/MCU service; validate via the PC simulator first.
- Forgetting to enable DFS + elm-chan fatfs in menuconfig — model files on the USB stick won't be mountable.
- Assuming an RV110x or RK35xx demo will compile for RK2118. The runtime, toolchain, and several op mappings differ — start from `rt-thread/components/rknpu/examples/`.
- Expecting dynamic shape, matmul, or general-API demos to work — they don't on RK2118 (mini runtime, same restrictions as RV110x).
