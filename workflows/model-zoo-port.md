# Workflow: Model Zoo porting / demo adaptation

## Read first

1. Always Read
2. `references/10_model_zoo_workflows.md`
3. `references/09_troubleshooting.md`
4. Conversion issues → `workflows/convert-model.md`

## Steps

0. If the user only asks whether **missing Model Zoo demo means unsupported** — answer **no** (FAQ 1.8 / `gotchas/high-cost.md` #10). Proceed to convert + op check; do not refuse the network. No LLM zoo demo (FAQ 1.9) either.
1. Use **RKNPU2** tree only (`cpp/rknpu2`); never port `rknpu1` demos into Toolkit2 / `librknnrt` stack.
2. Copy the demo's **exact** preprocess/postprocess contract (letterbox, color, anchors, NMS, dequant).
3. Prefer the closest zoo model family; keep shared `utils` / `3rdparty` (RGA, JPEG) wiring.
4. Build scripts: `build-linux.sh` / `build-android.sh` with correct arch and target. Linux toolchains: aarch64 Linaro 6.3.1, armhf gcc-arm-8.3, RV110x arm-rockchip830 uclibc. Android targets are only `rk356x`/`rk3576`/`rk3588`. `-r` disables RGA and is invalid on RV1103/RV1103B/RV1106; `-j` avoids OpenCV/libjpeg conflicts.
5. If demo OK / user model wrong: export path first (official export fork/scripts), then quant accuracy workflow.
6. Board Python path: `references/19_toolkit_lite2_board_python.md` + `assets/lite2_python_infer.py`.

## Done when

- Pre/post process contract and RKNPU2/lib choice stated.
- Build/target flags match board OS/arch matrix.
