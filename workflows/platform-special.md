# Workflow: Platform-specific deploy (branch table)

## Read first

Always Read, then **one** branch below (do not preload all).

| Signal | Read | Assets / notes |
|---|---|---|
| RK2118 / RT-Thread / serial / no adb | `references/14_rk2118_rtos_deploy.md` + `references/05_runtime_c_api.md` | zero-copy templates; runtime **not** from regular `rknpu2/runtime` |
| RV1103 / RV1106 / B variants | `references/01_platform_matrix.md` + `references/05_runtime_c_api.md` + `references/04_board_debug_rknn_server.md` | `librknnmrt.so`, mini-runtime, B-model incompatibility |
| RV1126B | `references/01_platform_matrix.md` + `references/05_runtime_c_api.md` | **normal** `librknnrt.so`, general API allowed — not an RV110x mini-runtime board |
| Android / JNI / HIDL / APK | `references/12_android_deploy.md` + `references/05_runtime_c_api.md` | CMake template; HIDL may drop matmul/custom-op |
| Board Python / RKNNLite / Lite2 | `references/19_toolkit_lite2_board_python.md` | `assets/lite2_python_infer.py` |
| WSL / USB adb on Windows | `references/08_wsl.md` + `references/04_board_debug_rknn_server.md` | Ethernet adb preferred |
| Perf / memory / SRAM / multi-core / batch | `references/06_performance_memory.md` + optionally `references/13_advanced_features.md` | RGA zero-copy template; core-mask gates |
| Encryption / multi-res shared weights / QAT entry / cacheable sync | `references/13_advanced_features.md` | advanced only after basic path works |
| Custom op / onnx_edit / sparsity / functions map | `workflows/graph-advanced.md` → `21`/`22`/`23` | not a deploy transport branch — hand off |
| Smallest official smoke before large app | `references/20_official_examples_and_smoke_paths.md` | common_test / benchmark / Android demo / functions/* |

## Steps

1. Match the signal row; open only those refs.
2. State runtime library + API family + deploy transport (adb / serial / package).
3. Hand off to convert / runtime-c / board-debug workflows when the ask spans multiple stages.

## Done when

- Platform-specific constraints named (lib, API, transport, model interchange).
