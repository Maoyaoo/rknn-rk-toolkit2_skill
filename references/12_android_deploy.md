# Android Deployment

Use this file when the target is an Android build on RK356x/RK3576/RK3588 (the SoCs Android firmware commonly ships for). RV1103/RV1106/RK2118 are not Android targets — use the Linux/uClibc path for those.

## Runtime library location

| Item | Path |
|---|---|
| `rknn_server` (connected debug) | `/vendor/bin/rknn_server` |
| Runtime (64-bit) | `/vendor/lib64/librknnrt.so` |
| Runtime (32-bit) | `/vendor/lib/librknnrt.so` |

Update both `rknn_server` and `librknnrt.so` together; version mismatch between them is the most common cause of `RKNN_ERR_DEVICE_UNAVAILABLE` and "abnormal server" errors. See `references/04_board_debug_rknn_server.md` for the `adb push` and log-level commands.

## Two ways to call the C API on Android (RKNNRT API Ref §3.3)

Android apps can integrate RKNN in two modes — **pick deliberately**, since switching later affects packaging:

### 1. Direct link to `librknnrt.so` (recommended for non-CTS apps)

- Shorter call chain, better performance.
- **Includes** matmul and custom op APIs.
- Push the library to `/vendor/lib64/librknnrt.so` (or `/vendor/lib/` for 32-bit).

### 2. HIDL via `librknn_api_android.so` (required for CTS/VTS devices)

- Goes through Android's HIDL/NNAPI HAL plumbing.
- **Required** for devices that must pass CTS/VTS certification.
- **Does NOT include** matmul or custom op APIs.
- The HIDL stack produces these files at firmware build time:

  ```
  /vendor/lib/librknn_api_android.so
  /vendor/lib/librknnhal_bridge.rockchip.so
  /vendor/lib64/librknn_api_android.so
  /vendor/lib64/librknnhal_bridge.rockchip.so
  /vendor/lib64/rockchip.hardware.neuralnetworks@1.0.so
  /vendor/lib64/rockchip.hardware.neuralnetworks@1.0-adapter-helper.so
  /vendor/lib64/hw/rockchip.hardware.neuralnetworks@1.0-impl.so
  /vendor/bin/hw/rockchip.hardware.neuralnetworks@1.0-service
  ```

- Source lives at `vendor/rockchip/hardware/interfaces/neuralnetworks/` inside the RK3562/RK3566/RK3568/RK3576/RK3588 Android SDK. Rebuild standalone with `mmm vendor/rockchip/hardware/interfaces/neuralnetworks/-j8`.
- App links only `librknn_api_android.so`; the rest is the HAL service stack.

For non-CTS devices (most custom deployments, kiosks, industrial screens), prefer mode 1 — the perf delta is measurable and you keep access to matmul + custom ops.

## Build paths

### Model Zoo Android demo

```bash
export ANDROID_NDK_PATH=~/opts/ndk/android-ndk-r19c
./build-android.sh -t <rk356x|rk3576|rk3588> -a <arm64-v8a|armeabi-v7a> \
    -d <demo> [-b Debug|Release] [-m] [-r] [-j]
```

Notes:

- Android targets supported by Model Zoo are `rk356x`, `rk3576`, `rk3588` only.
- NDK r18/r19 are the common stable choices in the upstream examples. This skill uses **r19c** in examples for consistency; newer NDKs may work but can require bumping `ANDROID_PLATFORM`.
- `-r` disables RGA and falls back to CPU resize. RGA is the default on Android; only pass `-r` if RGA misbehaves on your firmware.
- `-j` disables `libjpeg_turbo`. Use it when you also link OpenCV — OpenCV's bundled JPEG conflicts with libjpeg_turbo and causes a segfault (see FAQ 1.11 / Troubleshooting).
- `-m` enables ASAN and requires `-b Debug`.

### Native APK demo (camera + JNI)

The official RKNN SDK ships an Android APK demo for YOLOv5 in its example set (V2.3.2 era). Use it as the structural reference when you have the upstream SDK checkout. Two parts:

- **Java** (`com.rockchip.gpadc.demo`): camera preview, JNI calls, result rendering.
- **JNI** (`app/src/main/cpp/`): loads `librknnrt.so`, runs the RKNN model, returns detections to Java.

Build with Android Studio 2021.1.2 / 1.3.1 or higher. RGA version pairing: `librga.so` >1.4.0 requires the RGA multicore driver >=1.2.0, while `librga.so` >1.9.0 also supports the older RGA/RGA2 drivers listed in `rknn_doc/Rockchip_FAQ_RGA_CN.md`.

## Common Android pitfalls

### Duplicate JNI .so in mergeDebugNativeLibs

```
2 files found with path 'lib/arm64-v8a/librga.so' from inputs:
  - .../merged_jni_libs/debug/out/arm64-v8a/librga.so
  - .../cxx/Debug/.../obj/arm64-v8a/librga.so
```

Cause: `jniLibs` and CMake `IMPORTED` targets both ship the same `.so`. Fix by adding to `app/build.gradle`:

```groovy
android {
    sourceSets {
        main { jniLibs.srcDirs = ['libs'] }
    }
}
```

### `E/SurfaceView: Exception configuring surface` / `setParameters failed`

Cause: requested camera preview size not supported by the device. Edit `CAMERA_PREVIEW_WIDTH` / `CAMERA_PREVIEW_HEIGHT` in `app/src/main/java/com/rockchip/gpadc/demo/rga/HALDefine.java` to one of the sizes the device enumerates in `Camera Supported Preview Size`.

### Permissions

Camera, Read/Write External Storage must be granted at runtime on Android 6+.

### SELinux blocking rknn_server

If the board-side `rknn_server` process exits immediately with permission denied errors and `adb logcat` shows SELinux denials, the device is running with SELinux enforcing. Temporarily disable it for debugging:

```bash
adb shell setenforce 0
```

This is a debug-only switch — re-enabling (`setenforce 1`) is required for production. A proper fix is to package `rknn_server` into a vendor partition with the right SELinux policy, but for connected-debugging sessions `setenforce 0` is the practical workaround Rockchip documents (rknn_server_proxy.md problem 6).

### 32-bit vs 64-bit

Use `arm64-v8a` for 64-bit Android (default on RK3588). `armeabi-v7a` is supported for legacy 32-bit firmware, but the 64-bit `librknnrt.so` is the maintained one — match the ABIs listed in `android { defaultConfig { ndk { abiFilters } } }` to avoid packing both.

## Connected debugging from a PC

- ADB over USB works directly on most Windows/Linux hosts. On WSL see `references/08_wsl.md` (prefer Ethernet `adb connect <board_ip>:5555`).
- `rknn_server` must be running on the device and its version must match Toolkit2 and `librknnrt.so`.
- ARM64 Toolkit2 targets **Linux** arm64 boards (local conversion/runtime, ONNX/PyTorch only, no adb connected debug). It is not an Android userspace host — convert Android targets with x86 Toolkit2 on a host PC.

## What stays the same as Linux

- The C API (`rknn_api.h`), general-API vs zero-copy choice, MATMUL, custom op, dynamic shape support all match the Linux section of `references/05_runtime_c_api.md`. The differences are packaging/ABI/library path, not the API surface.
