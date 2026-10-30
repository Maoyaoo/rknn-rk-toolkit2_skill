#!/usr/bin/env python3
from __future__ import annotations

import argparse
import platform
from pathlib import Path

import numpy as np
from rknnlite.api import RKNNLite


COMPATIBLE_NODE = Path("/proc/device-tree/compatible")


def detect_host() -> str:
    if platform.system() != "Linux" or platform.machine() != "aarch64":
        return f"{platform.system()}-{platform.machine()}"
    try:
        text = COMPATIBLE_NODE.read_text(errors="ignore").replace("\x00", "\n").lower()
    except OSError:
        return "unknown-aarch64"
    if "rk3588" in text:
        return "rk3588"
    if "rk3576" in text:
        return "rk3576"
    if "rk3562" in text:
        return "rk3562"
    if "rk3566" in text or "rk3568" in text:
        return "rk3566_rk3568"
    if "rv1126b" in text:
        return "rv1126b"
    return "unknown-aarch64"


def select_core_mask(host: str, core: str):
    if core == "none" or host not in {"rk3576", "rk3588"}:
        return None
    if core == "0":
        return RKNNLite.NPU_CORE_0
    if core == "1":
        return RKNNLite.NPU_CORE_1
    if core == "all":
        return RKNNLite.NPU_CORE_ALL
    raise SystemExit(f"unsupported --core value: {core}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run RKNNLite inference on board")
    parser.add_argument("--model", required=True)
    parser.add_argument("--input", required=True, help="input .npy file")
    parser.add_argument("--data-format", default=None, help="example: nhwc or nchw")
    parser.add_argument("--core", choices=["none", "0", "1", "all"], default="none",
                        help="core mask (only honored on RK3576/RK3588; default: auto)")
    args = parser.parse_args()

    host = detect_host()
    rknn = RKNNLite()
    ret = rknn.load_rknn(args.model)
    if ret != 0:
        raise SystemExit(f"load_rknn failed: {ret}")

    core_mask = select_core_mask(host, args.core)
    if core_mask is None:
        ret = rknn.init_runtime()
    else:
        ret = rknn.init_runtime(core_mask=core_mask)
    if ret != 0:
        raise SystemExit(f"init_runtime failed: {ret}")

    input_data = np.load(args.input)
    kwargs = {}
    if args.data_format:
        kwargs["data_format"] = [args.data_format]
    outputs = rknn.inference(inputs=[input_data], **kwargs)
    for index, output in enumerate(outputs):
        print(f"output[{index}] shape={output.shape} dtype={output.dtype}")
    rknn.release()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
