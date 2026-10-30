# RKNN Model Zoo Workflows

Use this file when adapting `rknn_model_zoo` examples, building C demos, or porting a custom model into an existing Model Zoo pipeline. Paths below assume the user has the `rknn_model_zoo` repo checked out locally (the skill does not bundle it).

## Repository roles

- `examples/<demo>/python/convert.py`: model-to-RKNN conversion.
- `examples/<demo>/python/*.py`: Python inference and postprocess.
- `examples/<demo>/cpp`: C/C++ board demo.
- `utils/`: shared image/audio/file utilities.
- `3rdparty/`: runtime libraries, RGA, JPEG, audio dependencies, and headers.
- `datasets/`: calibration dataset file examples.

## Demo families (V2.3.2-era zoo)

| Task | Example directories |
|---|---|
| Classification | `mobilenet`, `resnet` |
| Object detection | `yolov5`, `yolov6`, `yolov7`, `yolov8`, `yolov10`, `yolo11`, `yolox`, `ppyoloe`, `yolo_world`, `RetinaFace` |
| Segmentation | `deeplabv3`, `ppseg`, `yolov5_seg`, `yolov8_seg`, `mobilesam` |
| Pose / OBB | `yolov8_pose`, `yolov8_obb` |
| OCR / plate | `PPOCR`, `LPRNet` |
| Audio / speech | `wav2vec2`, `whisper`, `zipformer`, `yamnet`, `mms_tts` |
| Text / embedding | `clip`, `lite_transformer` |

The per-demo `README.md` "Current Support Platform" section is authoritative; not every demo supports every SoC (this is a curated sample set, not an allow-list).

## Conversion pattern

Most `convert.py` scripts accept:

```bash
python convert.py <model_path> <platform> [i8|fp] [output.rknn]
```

Common platforms include `rk3562`, `rk3566`, `rk3568`, `rk3576`, `rk3588`, `rv1103`, `rv1106`, `rv1126b`. Some demos also list RKNPU1 targets (`rk1808`, `rv1109`, `rv1126`); do not mix those with this RKNPU2 skill unless the user explicitly targets RKNPU1.

Model-specific preprocessing matters:

- YOLO: often `mean_values=[[0,0,0]]`, `std_values=[[255,255,255]]`.
- PPOCR-Det/MobileSAM encoder: ImageNet-style means/stds.
- RetinaFace: Caffe-style `[104,117,123]`.
- Audio/text/transformer demos often use no image mean/std and default FP16.
- Some demos use `op_target` to force selected ops to CPU; preserve only with a documented reason.

## Linux build

```bash
./build-linux.sh -t <target> -a <arch> -d <demo> [-b Debug|Release] [-m] [-r] [-j]
```

Important options:

- `-t rk356x` means RK3562/RK3566/RK3568.
- `-t rv1106` is used for RV1103/RV1106 builds.
- `-r` disables RGA and uses CPU resize. It is invalid for RV1103/RV1103B/RV1106.
- `-j` disables libjpeg_turbo to avoid JPEG conflicts with OpenCV.
- `-m` enables ASAN and requires Debug builds.

### Toolchain setup

Before the first Linux build, export `GCC_COMPILER`. Rockchip's Model Zoo compilation guide pins **different** toolchains per architecture:

- aarch64: **Linaro 6.3.1-2017.05** → `gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu`
- armhf (RK35xx): **gcc-arm-8.3-2019.03** → `gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf`
- armhf-uclibc (RV1103/RV1106): **arm-rockchip830-linux-uclibcgnueabihf** → `.../bin/arm-rockchip830-linux-uclibcgnueabihf`

Do not use the Linaro 6.3 armhf toolchain for RK35xx armhf builds — the upstream guide calls for gcc-arm-8.3 there. Either `export GCC_COMPILER=...` in your shell or edit the variable at the top of `build-linux.sh`.

## Android build

```bash
export ANDROID_NDK_PATH=~/opts/ndk/android-ndk-r19c
./build-android.sh -t <target> -a <arm64-v8a|armeabi-v7a> -d <demo> [-b Debug|Release] [-m] [-r] [-j]
```

Android demos support `rk356x`, `rk3576`, and `rk3588`. Upstream examples commonly use **NDK r18/r19**; the examples in this skill use **r19c** for consistency. Download from `https://dl.google.com/android/repository/android-ndk-r19c-linux-x86_64.zip`, then either `export ANDROID_NDK_PATH=Projects/android-ndk-r19c` or edit the variable at the top of `build-android.sh`.

## Install directory layout and deploy workflow

After `build-linux.sh` / `build-android.sh` succeeds, artifacts land under `install/<soc>_<os>_<arch>/<demo>/`:

```
install/
├── rk356x_linux_aarch64/
│   └── rknn_yolov5_demo/
│       ├── lib/                    # runtime + 3rd-party .so dependencies
│       ├── model/                  # .rknn + test image (e.g. bus.jpg)
│       └── rknn_yolov5_demo        # executable (same name as the folder)
└── rk3588_android_arm64-v8a/
    └── rknn_yolov5_demo/
        ├── lib/
        ├── model/
        └── rknn_yolov5_demo
```

Push the whole folder to `/data/`, run, then pull the result image:

```bash
# Android example (RK3588)
adb root
adb push install/rk3588_android_arm64-v8a/rknn_yolov5_demo /data/

# Linux example (RK356x)
adb push install/rk356x_linux_aarch64/rknn_yolov5_demo /data/

# On the board
adb shell
cd /data/rknn_yolov5_demo/
export LD_LIBRARY_PATH=./lib
./rknn_yolov5_demo model/yolov5s_relu.rknn model/bus.jpg

# From the host: pull the result
adb pull /data/rknn_yolov5_demo/out.png .
```

The demo README uses `model/bus.jpg` as the sample input; `out.png` is written next to the executable. Both the model and image are command-line arguments, not built-in defaults.

**RV1103/RV1106 note:** do NOT reuse the generic `export LD_LIBRARY_PATH=./lib` form above. On RV1103/RV1106 the official examples require an **absolute** path to the pushed demo's `lib/`:

```bash
export LD_LIBRARY_PATH=/userdata/rknn_yolov5_demo/lib
```

The upstream examples README calls this out explicitly for RV1103/RV1106; relative `LD_LIBRARY_PATH` is fine on RK356x/RK3588 but is not the safe default on RV110x.

## Python demo: verify the original model first

`yolov5.py` and most Python demos accept either the converted `.rknn` (with `--target`) or the original `.onnx` (without `--target`) as `--model_path`. Run on the host with the ONNX first to confirm the model + postprocess are correct before chasing conversion/quantization issues:

```bash
cd rknn_model_zoo/examples/yolov5/python
# host-side ONNX sanity check
python yolov5.py --model_path ../model/yolov5s_relu.onnx --img_show
# connected-inference RKNN run on the board
python yolov5.py --model_path ../model/yolov5s_relu.rknn --target rk3588 --img_show
```

## Dataset accuracy evaluation (COCO MAP)

Model Zoo ships per-dataset downloaders under `datasets/`. For YOLOv5 COCO eval:

```bash
# one-time: fetch val2017
cd rknn_model_zoo/datasets/COCO
python download_eval_dataset.py

# need pycocotools for MAP computation
pip install pycocotools

# run the eval
cd rknn_model_zoo/examples/yolov5/python
python yolov5.py \
    --model_path ../model/yolov5s_relu.rknn \
    --target rk3588 \
    --img_folder ../../../datasets/COCO/val2017 \
    --coco_map_test
```

Other demos follow the same pattern — `datasets/<set>/download_eval_dataset.py` + a demo-specific `--<set>_test` flag. Use this to compare quantized vs FP MAP when accuracy matters.

## C demo structure

Model Zoo C demos usually:

1. Load/init RKNN.
2. Query `RKNN_QUERY_IN_OUT_NUM`.
3. Query and cache input/output attrs.
4. Derive input width/height/channel from attrs.
5. Reproduce exact preprocessing with `image_utils`/RGA/CPU resize/letterbox.
6. Run general API or zero-copy depending on target.
7. Postprocess with model-specific output layout, quant params, anchors, labels, and thresholds.

Do not replace this with a generic RKNN skeleton unless the model is trivial.

## RV1103/RV1106 differences

- Model Zoo has no separate `RV1106_RV1103` tree. The build script prints the supported subset: `rv1106_rv1103 only support: mobilenet and yolov5/6/7/8/x`; always check the demo's `README.md` "Current Support Platform" before porting.
- CMake selects `<demo>/cpp/rknpu2/<demo>_rv1106_1103.cc` (for example `yolov5/cpp/rknpu2/yolov5_rv1106_1103.cc`).
- On the Runtime side, `rknpu2/examples/RV1106_RV1103/` contains the RV110x-specific C demos (`rknn_mobilenet_demo`, `rknn_yolov5_demo`).
- Runtime is `librknnmrt.so`.
- Zero-copy is the default style.
- Some demos allocate DMA buffers for RGA.
- `LD_LIBRARY_PATH` must use an absolute path such as `/userdata/lib`, not `./lib`.
- Available demos are limited by memory; large models are intentionally absent.

## YOLO-specific checks

- Export from the Model Zoo-recommended fork or patch path.
- Preserve whether sigmoid is inside the model. Missing sigmoid can produce confidences greater than 1 or too many boxes.
- Preserve anchors for YOLOv5/YOLOv7.
- Match letterbox color, resize dimensions, NMS threshold, box threshold, class list, and output tensor order.
- Quantized postprocess should use output `zp` and `scale`; float conversion can be avoided when code handles int8 directly.

## When porting custom models

Start from the closest demo by model family, not from the smallest code sample. Keep conversion config, preprocess, output decode, and CMake target selection together in the same review.
