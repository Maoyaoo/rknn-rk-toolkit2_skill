#!/usr/bin/env python3
"""RKNN-Toolkit2 accuracy_analysis template.

Generates a side-by-side dump of every layer's output for:
  - the original (float) simulator,
  - the quantized simulator,
  - (optionally) the board NPU runtime,
under a fixed input, then points at where cosine/euclidean error first spikes.

Workflow (per references/03_quantization_accuracy.md):
  1. Build the RKNN model exactly as you deploy it (same target_platform, mean/std, dtype).
  2. Run accuracy_analysis() with one representative sample.
  3. Inspect the output directory layout:
       snapshot/golden/      — FP reference layer outputs
       snapshot/simulator/   — quantized simulator layer outputs
       snapshot/runtime/     — board NPU layer outputs (only with --connected)
       snapshot/error_analysis.txt — per-layer cosine/euclidean table
  4. Compare `single` vs `entire` columns to isolate layer-local errors from
     accumulated errors (see references/03_quantization_accuracy.md).

Constraints:
  - Must run after build() or hybrid_quantization_step2(); not after load_rknn().
  - If target is unset and the source model is QAT-quantized, the call fails.

Source: distilled from the RKNN Toolkit2 accuracy_analysis example in the V2.3.2 SDK.
The original downloads resnet50v2 and shows Top-5; this template
strips the demo-specific parts and exposes the analysis knobs.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from rknn.api import RKNN


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--onnx", required=True, help="source ONNX model")
    parser.add_argument("--target", required=True, help="target_platform, e.g. rk3588, rk3576, rk3566")
    parser.add_argument("--input", required=True, help="calibration/sample image or .npy")
    parser.add_argument("--dataset", required=True, help="dataset.txt for quantization build")
    parser.add_argument("--mean", default="0,0,0", help="mean_values, must match training preprocessing")
    parser.add_argument("--std", default="255,255,255", help="std_values, must match training preprocessing")
    parser.add_argument("--dtype", default="w8a8", help="quantized_dtype, e.g. w8a8, w4a16, w8a16")
    parser.add_argument("--algorithm", default="normal", help="normal, mmse, kl_divergence, gdq")
    parser.add_argument("--out-dir", default="./snapshot", help="accuracy_analysis output dir")
    parser.add_argument("--build-only", action="store_true", help="build and skip accuracy_analysis")
    parser.add_argument("--connected", action="store_true",
                        help="also dump board NPU outputs (requires connected device + rknn_server)")
    args = parser.parse_args()

    def floats(text: str) -> list[float]:
        return [float(v) for v in text.split(",") if v.strip()]

    rknn = RKNN(verbose=True, verbose_file="rknn_accuracy.log")
    try:
        ret = rknn.config(
            mean_values=[floats(args.mean)],
            std_values=[floats(args.std)],
            target_platform=args.target,
            quantized_dtype=args.dtype,
            quantized_algorithm=args.algorithm,
            quantized_method="channel",
        )
        if ret != 0:
            print(f"config failed: {ret}", file=sys.stderr)
            return ret

        ret = rknn.load_onnx(model=args.onnx)
        if ret != 0:
            print(f"load_onnx failed: {ret}", file=sys.stderr)
            return ret

        ret = rknn.build(do_quantization=True, dataset=args.dataset)
        if ret != 0:
            print(f"build failed: {ret}", file=sys.stderr)
            return ret

        if args.build_only:
            print("build-only: skipping accuracy_analysis()")
            return 0

        # Dump every layer's output for golden (float) vs simulator (quantized),
        # plus board runtime outputs when --connected is set.
        analysis_kwargs = {"inputs": [args.input], "output_dir": args.out_dir}
        if args.connected:
            analysis_kwargs["target"] = args.target
        ret = rknn.accuracy_analysis(**analysis_kwargs)
        if ret != 0:
            print(f"accuracy_analysis failed: {ret}", file=sys.stderr)
            return ret

        out = Path(args.out_dir)
        print(f"\nAccuracy snapshot written to: {out}")
        print("Compare per-layer outputs under:")
        print(f"  golden    (float):  {out / 'golden'}")
        print(f"  simulator (quant):  {out / 'simulator'}")
        if args.connected:
            print(f"  runtime   (board):  {out / 'runtime'}")
        print("Inspect the first layers with large cosine/euclidean error first;")
        print("then consider hybrid_quantization_step1/step2 or auto_hybrid.")
    finally:
        rknn.release()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
