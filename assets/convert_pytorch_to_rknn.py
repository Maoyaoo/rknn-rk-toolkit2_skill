#!/usr/bin/env python3
from __future__ import annotations

import argparse
from rknn.api import RKNN


def shape(text: str) -> list[int]:
    return [int(item) for item in text.replace("x", ",").split(",") if item.strip()]


def floats(text: str) -> list[float]:
    return [float(item) for item in text.split(",") if item.strip()]


def parse_op_target(items: list[str]) -> dict[str, str] | None:
    if not items:
        return None
    result: dict[str, str] = {}
    for item in items:
        if "=" not in item:
            raise SystemExit(f"--op-target expects NAME=cpu, got: {item}")
        name, target = item.split("=", 1)
        result[name] = target
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert PyTorch to RKNN with RKNN-Toolkit2")
    parser.add_argument("--model", required=True)
    parser.add_argument("--input-size", required=True, help="example: 1,3,224,224")
    parser.add_argument("--out", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--dataset")
    parser.add_argument("--no-quant", action="store_true")
    parser.add_argument("--mean")
    parser.add_argument("--std")
    parser.add_argument("--rgb2bgr", action="store_true")
    parser.add_argument("--dtype", default="w8a8", help="V2.3.2 examples: w8a8, w4a16, w8a16, w16a16i")
    parser.add_argument("--algorithm", default="normal", help="normal, mmse, kl_divergence, gdq")
    parser.add_argument("--method", default="channel", help="layer, channel, group32...group256")
    parser.add_argument("--auto-hybrid", action="store_true")
    parser.add_argument("--op-target", action="append", default=[], help="force op target, e.g. op_name=cpu")
    args = parser.parse_args()

    do_quant = not args.no_quant
    if (do_quant or args.auto_hybrid) and not args.dataset:
        raise SystemExit("--dataset is required when quantization or auto-hybrid is enabled")

    config_kwargs = {
        "target_platform": args.target,
        "quantized_dtype": args.dtype,
        "quantized_algorithm": args.algorithm,
        "quantized_method": args.method,
    }
    if args.mean:
        config_kwargs["mean_values"] = [floats(args.mean)]
    if args.std:
        config_kwargs["std_values"] = [floats(args.std)]
    # quant_img_RGB2BGR is meaningful only when mean/std image loading is used.
    config_kwargs["quant_img_RGB2BGR"] = args.rgb2bgr
    op_target = parse_op_target(args.op_target)
    if op_target:
        config_kwargs["op_target"] = op_target

    rknn = RKNN(verbose=True, verbose_file="rknn_build.log")
    try:
        ret = rknn.config(**config_kwargs)
        if ret != 0:
            raise SystemExit(f"config failed: {ret}")
        ret = rknn.load_pytorch(model=args.model, input_size_list=[shape(args.input_size)])
        if ret != 0:
            raise SystemExit(f"load_pytorch failed: {ret}")
        ret = rknn.build(do_quantization=do_quant, dataset=args.dataset, auto_hybrid=args.auto_hybrid)
        if ret != 0:
            raise SystemExit(f"build failed: {ret}")
        ret = rknn.export_rknn(args.out)
        if ret != 0:
            raise SystemExit(f"export_rknn failed: {ret}")
        print("done:", args.out)
    finally:
        rknn.release()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
