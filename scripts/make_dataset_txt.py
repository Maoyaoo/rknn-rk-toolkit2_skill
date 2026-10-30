#!/usr/bin/env python3
"""Build a dataset.txt for RKNN-Toolkit2 quantization calibration.

Toolkit2's quantization image loader accepts image files (jpg/png/bmp/webp)
and `.npy` arrays. Raw `.bin`/`.data` tensors are NOT supported by the loader,
so they are intentionally excluded here -- convert them to `.npy` first.

Each line of the output is a path. For multi-input models, list one file per
input on the same line, space-separated (e.g. `a.jpg b.npy`).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Extensions Toolkit2's quant loader can consume directly.
IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
NPY_EXTS = {".npy"}
SUPPORTED_EXTS = IMAGE_EXTS | NPY_EXTS


def collect_inputs(image_dir: Path, limit: int) -> list[Path]:
    found = sorted(p for p in image_dir.rglob("*") if p.suffix.lower() in SUPPORTED_EXTS)
    if limit > 0:
        found = found[:limit]
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image_dir", help="directory of calibration images or .npy tensors")
    parser.add_argument("-o", "--output", default="dataset.txt", help="output list file (default: dataset.txt)")
    parser.add_argument("--limit", type=int, default=0, help="take only the first N entries (0 = all)")
    parser.add_argument("--multi-input", action="store_true",
                        help="group every N inputs per line (use --inputs-per-line to set N)")
    parser.add_argument("--inputs-per-line", type=int, default=2,
                        help="number of files per line when --multi-input is set (default: 2)")
    args = parser.parse_args()

    if args.inputs_per_line < 1:
        return parser.error("--inputs-per-line must be >= 1")

    root = Path(args.image_dir).resolve()
    if not root.is_dir():
        return parser.error(f"not a directory: {root}")

    entries = collect_inputs(root, args.limit)
    if not entries:
        print(f"no supported files in {root}; supported: {sorted(SUPPORTED_EXTS)}", file=sys.stderr)
        return 1

    if args.multi_input:
        lines = [
            " ".join(str(p) for p in entries[i:i + args.inputs_per_line])
            for i in range(0, len(entries), args.inputs_per_line)
        ]
    else:
        lines = [str(p) for p in entries]

    Path(args.output).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {len(lines)} entries to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
