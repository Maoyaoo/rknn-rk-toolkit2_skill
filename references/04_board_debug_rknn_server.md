# rknn_server / Board Connected Debugging

`rknn_server` is a board-side proxy. PC Toolkit2 sends data over adb/USB or adb/TCP, then the server calls the board Runtime API and returns results.

Keep RKNN-Toolkit2, `rknn_server`, runtime library, driver/BSP, and generated RKNN model target aligned.

## Version gates

- Dynamic-shape RKNN connected inference requires `rknn_server` and Runtime >= 1.5.0.
- Connected accuracy analysis (`accuracy_analysis(target=<SoC>)`) on a model containing **custom ops** requires `rknn_server` >= 1.6.0 (User Guide §5.5.4.4).
- Models larger than 2GB require `rknn_server` >= 2.0.0b0.
- RV1103/RV1103B/RV1106/RV1106B small-memory boards should use `rknn_server` >= 2.1.0.
- RV1103/RV1103B/RV1106/RV1106B do not support `perf_debug=True` layer timing through connected inference.

## Runtime/server paths

| Platform | Server path | Runtime library |
|---|---|---|
| Android 64-bit | `/vendor/bin/rknn_server` | `/vendor/lib64/librknnrt.so` |
| Android 32-bit | `/vendor/bin/rknn_server` | `/vendor/lib/librknnrt.so` |
| Linux non-RV110x 64-bit | `/usr/bin/rknn_server` | `/usr/lib/librknnrt.so` or `/usr/lib64/librknnrt.so` |
| Linux non-RV110x 32-bit | `/usr/bin/rknn_server` | `/usr/lib/librknnrt.so` |
| RV1103/RV1106/RV1103B/RV1106B | `/oem/usr/bin/rknn_server` | `/oem/usr/lib/librknnmrt.so` |

If RV110x has another `librknnmrt.so` under `/usr/lib`, remove the stale copy or fix library search paths, then restart `rknn_server`.

## Version checks

```bash
adb shell 'strings /vendor/bin/rknn_server | grep -i "rknn_server version"'
adb shell 'strings /usr/bin/rknn_server | grep -i "rknn_server version"'
adb shell 'strings /oem/usr/bin/rknn_server | grep -i "rknn_server version"'
adb shell 'strings /usr/lib/librknnrt.so /usr/lib64/librknnrt.so /vendor/lib64/librknnrt.so 2>/dev/null | grep -i "librknnrt version"'
adb shell 'strings /oem/usr/lib/librknnmrt.so 2>/dev/null | grep -i "librknnmrt version"'
```

## Start/update command generation

Use `scripts/rknn_server_commands.py` from the `rknpu2` project root to print platform-specific commands.

Examples:

```bash
python scripts/rknn_server_commands.py --system linux --target rk3588 --arch aarch64 --logs
python scripts/rknn_server_commands.py --system linux --target rv1106 --arch armhf-uclibc --logs
python scripts/rknn_server_commands.py --system android --target rk3588 --arch arm64
```

## Quick-start helpers shipped on the board

Rockchip Linux firmware installs two wrapper scripts alongside `rknn_server`:

- Standard Linux: `/usr/bin/start_rknn.sh` and `/usr/bin/restart_rknn.sh`.
- RV110x: `/oem/usr/bin/start_rknn.sh` and `/oem/usr/bin/restart_rknn.sh`.

They start with default options, and `restart_rknn.sh` kills then restarts the server.

On a Linux board that already has matching RKNPU2 environment, prefer `restart_rknn.sh` over hand-written `killall`+`nohup`:

```bash
adb shell
restart_rknn.sh         # or: start_rknn.sh
# expect: "start rknn server, version: x.x.x"
ps -ef | grep rknn_server
```

If those scripts are missing or behave wrong (custom firmware, wrong `/usr/bin`), fall back to the manual `killall`+`nohup` sequences in §Debug logs below.

Android does NOT ship the wrapper scripts — start the server by hand:

```bash
adb shell
su
setenforce 0                                  # disable SELinux enforcement (debug only)
/vendor/bin/rknn_server &
# expect: "start rknn server, version: x.x.x"
```

`setenforce 0` is required on most Android firmware before `rknn_server` can bind its sockets — see `references/12_android_deploy.md`.

## RKNPU driver version

Check the in-kernel RKNPU driver (not the userspace Runtime):

```bash
adb shell 'dmesg | grep -i rknpu'
# look for: [drm] Initialized rknpu 0.8.8 20230428 for fdab0000.npu ...
```

Additional driver-version files (User Guide §2.2.1, §8.1.1) — try these if `dmesg` is unhelpful:

```bash
# RK3566/RK3568/RK3588/RK3562/RK3576 — debugfs path
adb shell 'cat /sys/kernel/debug/rknpu/version'
adb shell 'cat /sys/kernel/debug/rknpu/driver_version'

# RV1106/RV1103 series and RV1126B — proc path (User Guide §8.1.1)
# Note: RV1126B still uses librknnrt.so (normal runtime), not librknnmrt.so.
adb shell 'cat /proc/rknpu/version'
adb shell 'cat /proc/debug/rknpu/driver_version'
```

Recommended: **driver version >= 0.9.2**. If none of these produce a version, the firmware is likely a third-party build without the NPU driver. With kernel sources, rebuild with `CONFIG_ROCKCHIP_RKNPU=y` and reflash.

Official Rockchip firmware ships with the driver pre-installed — its absence is a third-party-firmware signal.

## RKNN model ↔ Runtime version compatibility (User Guide §10.1)

`RKNN_ERR_MODEL_INVALID` or `"Invalid RKNN model version N"` at `rknn_init` usually means the model was built with a newer Toolkit2 than the on-board Runtime supports. Compatibility matrix (V2.3.2 era):

| RKNN model version | Compatible Runtime |
|---|---|
| 1.2.0 | >= 1.2.0 and <= 1.5.0 |
| 1.3.0 | >= 1.3.0 and <= 1.5.0 |
| 1.4.0 | >= 1.4.0 and <= 1.5.0 |
| 1.5.0 | 1.5.0 only |
| 1.5.2 | >= 1.5.2 |
| 1.6.0 | >= 1.5.2 |
| 2.0.0 | >= 2.0.0 |
| 2.1.0 | >= 2.0.0 |

Fix by upgrading Toolkit2 + Runtime + rknn_server together to the latest release. Also a common cause of `"Invalid RKNN format"`: `target_platform` was not set in `rknn.config()` (default `rk3566`) and the model is being deployed on a different SoC.

## RKNN error codes (RKNNRT API Ref §5)

Every RKNN C API function returns one of these. Use them in logs to disambiguate:

| Code | Constant | Meaning | Typical cause |
|---|---|---|---|
| 0 | `RKNN_SUCC` | success | — |
| -1 | `RKNN_ERR_FAIL` | generic failure | NPU hang, Runtime bug, or SDK version mismatch. If inference takes > 20 s with this code, suspect NPU hang — upgrade SDK. |
| -2 | `RKNN_ERR_TIMEOUT` | execution timeout | NPU hung mid-inference. Reboot the board and upgrade Runtime. |
| -3 | `RKNN_ERR_DEVICE_UNAVAILABLE` | NPU device unavailable | `rknn_server` not running; `adb` cannot see device; Debian `adbd` not on port 5037; Toolkit2/Runtime/server version mismatch. |
| -4 | `RKNN_ERR_MALLOC_FAIL` | memory allocation failed | Board OOM. On RV110x: stop `rkipc`, see `references/06_performance_memory.md`. |
| -5 | `RKNN_ERR_PARAM_INVALID` | bad parameter | Wrong `rknn_input` / `rknn_output` field, wrong `rknn_query` size, etc. |
| -6 | `RKNN_ERR_MODEL_INVALID` | invalid RKNN model | Corrupted/stale `.rknn`; Toolkit2 newer than Runtime; wrong `target_platform`; RKNN model & Runtime version incompatible (see version matrix above). |
| -7 | `RKNN_ERR_CTX_INVALID` | invalid `rknn_context` | Used a context after `rknn_destroy`, or never initialized it. |
| -8 | `RKNN_ERR_INPUT_INVALID` | invalid `rknn_input` | Wrong `type` / `fmt` / `size` for the model's input attr. |
| -9 | `RKNN_ERR_OUTPUT_INVALID` | invalid `rknn_output` | Wrong `want_float` / `is_prealloc` configuration. |
| -10 | `RKNN_ERR_DEVICE_UNMATCH` | version mismatch | Toolkit2 vs Runtime vs rknn_server out of sync. Upgrade all three together. |
| -12 | `RKNN_ERR_INCOMPATILE_OPTIMIZATION_LEVEL_VERSION` | optimization level not supported by driver | Model built with `optimization_level=3` but driver too old. Lower the level or upgrade driver. |
| -13 | `RKNN_ERR_TARGET_PLATFORM_UNMATCH` | model & platform incompatible | `.rknn` was built for a different SoC than the board. Check `target_platform`. |

There is no `-11` in the V2.3.2 table — `-10` is followed directly by `-12`.

`-3` and `-6` together account for most field reports. The string `"server connect fail! ret = -9(ERROR_PIPE)"` in `init_runtime` also maps to the same root causes as `-3`.

## Debug logs

Android:

```bash
adb logcat -c
adb shell 'killall rknn_server'
adb shell 'setprop persist.vendor.rknn.server.log.level 5 && nohup /vendor/bin/rknn_server >/dev/null 2>&1 &'
adb logcat
adb shell 'killall rknn_server'
adb shell 'setprop persist.vendor.rknn.server.log.level 0 && nohup /vendor/bin/rknn_server >/dev/null 2>&1 &'
```

Linux non-RV110x:

```bash
adb shell 'mkdir -p /userdata'
adb shell 'killall rknn_server'
adb shell 'export RKNN_SERVER_LOGLEVEL=5 && nohup /usr/bin/rknn_server >/userdata/server.log 2>&1 &'
adb shell 'cat /userdata/server.log'
adb shell 'killall rknn_server'
adb shell 'export RKNN_SERVER_LOGLEVEL=0 && nohup /usr/bin/rknn_server >/userdata/server.log 2>&1 &'
```

RV110x:

```bash
adb shell 'mkdir -p /userdata'
adb shell 'killall rknn_server'
adb shell 'export RKNN_SERVER_LOGLEVEL=5 && nohup /oem/usr/bin/rknn_server >/userdata/server.log 2>&1 &'
adb shell 'cat /userdata/server.log'
```

## Known board issues

- Debian firmware can start `rknn_server` but fail connected inference if `adbd` is not listening on port 5037. Check with `adb shell 'netstat -anp | grep 5037'`.
- RV110x `failed to allocate fd`, `errno: 12`, or OOM usually means memory pressure. The User Guide suggests running `RkLunch-stop.sh` on the board to stop non-essential apps before connected inference, stopping `rkipc`, reducing model/input/output size, and upgrading `rknn_server` to >= 2.1.0.
- For RV110x accuracy analysis storage failures, set a larger dump path:

```bash
adb shell 'mkdir -p /userdata/dumps'
adb shell 'killall rknn_server'
adb shell 'export RKNN_DUMP_DIR=/userdata/dumps && nohup /oem/usr/bin/rknn_server >/userdata/server.log 2>&1 &'
```
