#!/usr/bin/env python3
from __future__ import annotations

import argparse


RV_TARGETS = {"rv1103", "rv1103b", "rv1106", "rv1106b"}


def is_rv110x(target: str) -> bool:
    return target.lower() in RV_TARGETS


def print_android(arch: str, logs: bool) -> None:
    server_src = f"runtime/Android/rknn_server/{arch}/rknn_server"
    lib_arch = "arm64-v8a" if arch == "arm64" else "armeabi-v7a"
    lib_dst = "/vendor/lib64/" if arch == "arm64" else "/vendor/lib/"
    print("adb root && adb remount")
    print(f"adb push {server_src} /vendor/bin/")
    print(f"adb push runtime/Android/librknn_api/{lib_arch}/librknnrt.so {lib_dst}")
    print("adb shell chmod +x /vendor/bin/rknn_server")
    if logs:
        print("adb logcat -c")
        print("adb shell 'killall rknn_server 2>/dev/null; setprop persist.vendor.rknn.server.log.level 5 && nohup /vendor/bin/rknn_server >/dev/null 2>&1 &'")
        print("adb logcat")
    else:
        print("adb shell 'killall rknn_server 2>/dev/null; nohup /vendor/bin/rknn_server >/dev/null 2>&1 &'")


def print_linux(target: str, arch: str, logs: bool) -> None:
    rv110x = is_rv110x(target)
    if rv110x:
        server_src = "runtime/Linux/rknn_server/armhf-uclibc/usr/bin/*"
        lib_src = "runtime/Linux/librknn_api/armhf-uclibc/librknnmrt.so"
        server_dst = "/oem/usr/bin/"
        lib_dst = "/oem/usr/lib/"
        server_bin = "/oem/usr/bin/rknn_server"
        ps_cmd = "ps | grep rknn_server"
    else:
        server_src = f"runtime/Linux/rknn_server/{arch}/usr/bin/*"
        lib_src = f"runtime/Linux/librknn_api/{arch}/librknnrt.so"
        server_dst = "/usr/bin/"
        lib_dst = "/usr/lib64/" if arch == "aarch64" else "/usr/lib/"
        server_bin = "/usr/bin/rknn_server"
        ps_cmd = "ps -ef | grep rknn_server"

    print(f"adb push {server_src} {server_dst}")
    print(f"adb push {lib_src} {lib_dst}")
    print(f"adb shell chmod +x {server_bin}")
    if logs:
        print("adb shell 'mkdir -p /userdata'")
        print(f"adb shell 'killall rknn_server 2>/dev/null; export RKNN_SERVER_LOGLEVEL=5 && nohup {server_bin} >/userdata/server.log 2>&1 &'")
        print(f"adb shell '{ps_cmd}'")
        print("adb shell 'cat /userdata/server.log'")
    else:
        print(f"adb shell 'killall rknn_server 2>/dev/null; nohup {server_bin} >/dev/null 2>&1 &'")
        print(f"adb shell '{ps_cmd}'")

    if rv110x:
        print("# RV110x accuracy-analysis dump workaround:")
        print("adb shell 'mkdir -p /userdata/dumps'")
        print(f"adb shell 'killall rknn_server 2>/dev/null; export RKNN_DUMP_DIR=/userdata/dumps && nohup {server_bin} >/userdata/server.log 2>&1 &'")


def main() -> int:
    parser = argparse.ArgumentParser(description="Print rknn_server update/start commands")
    parser.add_argument("--system", choices=["android", "linux"], required=True)
    parser.add_argument("--target", required=True, help="SoC target, e.g. rk3588, rk3576, rv1126b, rv1106")
    parser.add_argument("--arch", required=True, help="android: arm64/arm; linux: aarch64/armhf/armhf-uclibc")
    parser.add_argument("--logs", action="store_true", help="start rknn_server with verbose logs")
    args = parser.parse_args()

    print("# Run from the rknpu2 project root")
    print("adb devices")
    if args.system == "android":
        if args.arch not in {"arm64", "arm"}:
            raise SystemExit("android --arch must be arm64 or arm")
        print_android(args.arch, args.logs)
    else:
        if args.arch not in {"aarch64", "armhf", "armhf-uclibc"}:
            raise SystemExit("linux --arch must be aarch64, armhf, or armhf-uclibc")
        if is_rv110x(args.target) and args.arch != "armhf-uclibc":
            print("# Note: RV1103/RV1106 use armhf-uclibc runtime/server paths.")
        print_linux(args.target, args.arch, args.logs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
