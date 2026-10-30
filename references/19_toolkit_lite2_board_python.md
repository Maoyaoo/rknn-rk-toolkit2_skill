# RKNN-Toolkit-Lite2 Board Python

Use this file when the user wants **board-side Python inference** on an already converted `.rknn` model. This is the right path for RKNNLite / `rknnlite.api.RKNNLite`, not the PC-side Toolkit2 conversion API.

## What Lite2 is for

`RKNN-Toolkit-Lite2` is the Python deployment interface for Rockchip NPU boards. The usual split is:

- **PC / host:** use `rknn.api.RKNN` (Toolkit2) to convert source models into `.rknn`
- **board / device:** use `rknnlite.api.RKNNLite` (Toolkit-Lite2) to load `.rknn` and run inference in Python

Do not treat Lite2 as a replacement for host-side conversion. It is the board-side runtime-facing Python layer.

## When to use Lite2

- The user asks for board-side Python inference
- The user already has a `.rknn` and wants a quick Python smoke test on the board
- The user wants a Python path before rewriting the deployment in C/C++
- The user references `RKNNLite`, `rknnlite.api`, or `Toolkit-Lite2`

Prefer C/C++ Runtime for production performance-sensitive deployment. Lite2 is excellent for smoke tests, integration bring-up, and Python-side app logic.

## Board environment

- OS: Debian 10 / 11 (aarch64). Python 3.7–3.12.
- Install the wheel from the SDK's `rknn-toolkit-lite2/packages/` directory that matches the board's `cpXX` ABI, for example `pip3 install rknn_toolkit_lite2-x.y.z-cp38-cp38-manylinux_2_17_aarch64.manylinux2014_aarch64.whl`.
- Runtime deps: `numpy`, `ruamel.yaml`, `psutil`; the examples also use OpenCV (`python3-opencv`). The User Guide prints `psutils`, but the wheel metadata requires **`psutil`** — install that one.
- `init_runtime(core_mask=...)` core selection is only effective on RK3576 / RK3588. Leave it at `NPU_CORE_AUTO` elsewhere.
- Check whether a `.rknn` is runnable on the board before debugging it: `RKNNLite.list_support_target_platform(rknn_model="model.rknn")` returns the model's supported platforms; with no argument it prints the platforms Lite2 supports.

## Core flow

```python
from rknnlite.api import RKNNLite

rknn = RKNNLite()
rknn.load_rknn("model.rknn")
rknn.init_runtime()
outputs = rknn.inference(inputs=[input_ndarray])
rknn.release()
```

The local template in this skill is:

- `assets/lite2_python_infer.py`

Use that as the starting point instead of rewriting the flow from scratch.

## Supported examples in upstream Lite2

The Lite2 package ships examples for:

- `examples/resnet18/`
- `examples/dynamic_shape/`

These are useful for checking:

- basic board-side Python inference
- `core_mask` usage
- dynamic-shape board-side behavior on supported SoCs

## Platform and core-mask notes

- `RKNNLite.NPU_CORE_*` tuning mainly matters on multi-core SoCs such as RK3588 and RK3576.
- For single-core or non-supported SoCs, do not force a core mask by default.
- Lite2 examples in this repo already reflect that pattern; `assets/lite2_python_infer.py` only applies explicit core masks on RK3588 / RK3576.

## Dynamic shape note

Lite2 does have dynamic-shape examples upstream, but dynamic-shape support still depends on:

- how the `.rknn` was built
- the target SoC
- the runtime / board software version

If the user asks about dynamic-shape behavior on board, pair this file with:

- `references/11_dynamic_shape_and_mem_reuse.md`
- `references/15_rknn_doc_lookup.md` when exact runtime support needs confirmation

## What to verify in a Lite2 answer

When suggesting Lite2, make sure the answer also states:

- the model must already be converted to `.rknn`
- this is board-side runtime inference, not host-side conversion
- the target board/SoC still matters (`target_platform` used at build time must match)
- for performance-critical deployment, C/C++ Runtime remains the stronger default

## Common mistakes

- Suggesting `from rknn.api import RKNN` for board-side Python deployment
- Treating Lite2 as if it can convert ONNX/PyTorch directly on the board
- Forgetting that the `.rknn` must match the real board SoC
- Recommending core-mask tuning on unsupported/single-core platforms as if it were universal
