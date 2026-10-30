#!/usr/bin/env python3
from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys


RV_TARGETS = {"rv1103", "rv1103b", "rv1106", "rv1106b"}
PROC_DRIVER_TARGETS = RV_TARGETS | {"rv1126b"}


def run(cmd: list[str], timeout: int = 10) -> tuple[int, str]:
    try:
        proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        return proc.returncode, proc.stdout.strip()
    except FileNotFoundError:
        return 127, f"not found: {cmd[0]}"
    except subprocess.TimeoutExpired:
        return 124, "timeout"


def adb_shell(command: str, timeout: int = 10) -> str:
    code, out = run(["adb", "shell", command], timeout=timeout)
    return f"$ adb shell {command}\n(ret={code})\n{out}\n"


def print_tool_paths() -> None:
    for tool in [
        "adb",
        "cmake",
        "gcc",
        "aarch64-linux-gnu-gcc",
        "arm-linux-gnueabihf-gcc",
        "arm-rockchip830-linux-uclibcgnueabihf-gcc",
    ]:
        print(f"{tool}: {shutil.which(tool) or 'not found'}")


def probe_python() -> None:
    print("== Host ==")
    print("python:", sys.version.replace("\n", " "))
    print("platform:", platform.platform(), "machine:", platform.machine())
    print("\n== RKNN Python import ==")
    try:
        from rknn.api import RKNN

        print("rknn.api import: OK")
        rknn = RKNN()
        rknn.release()
    except Exception as exc:  # pragma: no cover - environment probe
        print("rknn.api import: FAIL", repr(exc))
    try:
        from rknnlite.api import RKNNLite  # noqa: F401

        print("rknnlite.api import: OK")
    except Exception as exc:  # pragma: no cover - environment probe
        print("rknnlite.api import: FAIL", repr(exc))


def probe_board(target: str | None) -> None:
    rv110x = target in RV_TARGETS if target else False
    code, out = run(["adb", "devices"])
    print("\n== adb devices ==\n" + out)
    # Distinguish Android from Linux. On Android getprop prints a numeric
    # release; on Linux getprop is absent and the shell prints "not_found".
    print(adb_shell("getprop ro.build.version.release 2>&1"))
    print(adb_shell("getprop ro.product.cpu.abi 2>&1"))
    print(adb_shell("uname -a"))
    print(adb_shell("cat /proc/device-tree/compatible 2>/dev/null | tr '\\0' '\\n'"))
    print(adb_shell("dmesg 2>/dev/null | grep -i rknpu | tail -n 20"))
    # `ps -ef` is unsupported on BusyBox (RV110x); fall back to plain `ps`
    # without leaking the resulting usage/error text.
    print(adb_shell("{ ps -ef 2>/dev/null || ps; } | grep '[r]knn_server'"))
    print(adb_shell("netstat -anp 2>/dev/null | grep '[5]037' || ss -lntp 2>/dev/null | grep '[5]037' || true"))

    if rv110x:
        checks = [
            "strings /oem/usr/bin/rknn_server 2>/dev/null | grep -i 'rknn_server version'",
            "strings /oem/usr/lib/librknnmrt.so 2>/dev/null | grep -i 'librknnmrt version'",
            "ls -l /usr/lib/librknnmrt.so /oem/usr/lib/librknnmrt.so 2>/dev/null",
            "df -h /tmp /userdata /oem 2>/dev/null",
        ]
        driver_checks = [
            "cat /proc/rknpu/version 2>/dev/null",
            "cat /proc/debug/rknpu/driver_version 2>/dev/null",
        ]
    else:
        checks = [
            "strings /usr/bin/rknn_server /vendor/bin/rknn_server 2>/dev/null | grep -i 'rknn_server version'",
            "strings /usr/lib/librknnrt.so /usr/lib64/librknnrt.so /vendor/lib64/librknnrt.so 2>/dev/null | grep -i 'librknnrt version'",
            "ls -l /usr/lib/librknnrt.so /usr/lib64/librknnrt.so /vendor/lib64/librknnrt.so 2>/dev/null",
        ]
        if target == "rv1126b":
            driver_checks = [
                "cat /proc/rknpu/version 2>/dev/null",
                "cat /proc/debug/rknpu/driver_version 2>/dev/null",
                "cat /sys/kernel/debug/rknpu/version 2>/dev/null",
            ]
        else:
            driver_checks = [
                "cat /sys/kernel/debug/rknpu/version 2>/dev/null",
                "cat /sys/kernel/debug/rknpu/driver_version 2>/dev/null",
            ]
    for check in checks + driver_checks:
        print(adb_shell(check))

    if rv110x:
        print(adb_shell("{ ps -ef 2>/dev/null || ps; } | grep '[r]kipc'"))


def main() -> int:
    parser = argparse.ArgumentParser(description="Probe RKNN Toolkit2 / RKNPU2 environment")
    parser.add_argument("--board", action="store_true", help="also probe an adb-connected board")
    parser.add_argument("--target", help="target SoC, e.g. rk3588, rv1106, rv1126b")
    args = parser.parse_args()

    probe_python()
    print("\n== Tools ==")
    print_tool_paths()
    if args.board:
        probe_board(args.target.lower() if args.target else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
