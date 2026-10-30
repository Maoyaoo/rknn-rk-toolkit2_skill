# Using RKNN-Toolkit2 in WSL

WSL2 Ubuntu 22.04 is a practical baseline for Toolkit2 installation, conversion, and quantization.

## Common dependency fix

If `ImportError: libGL.so.1: cannot open shared object file` occurs:

```bash
sudo apt update
sudo apt install libgl1-mesa-glx
echo 'export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/mesa:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

## Board debugging

Prefer Ethernet adb from WSL:

```bash
sudo apt update
sudo apt install adb
adb connect <board_ip>:<port>
adb devices
```

USB from Windows to WSL needs a bridge such as adbkit:

```bash
npm install --save adbkit
adbkit usb-device-to-tcp <device_id> -p <port>
adb connect <windows_host_ip>:<port>
```

## Practical caveats

- Verify the conversion stack before touching models: `python -c "from rknn.api import RKNN"` and `pip show rknn-toolkit2`.
- If adb sees the board from Windows but not WSL, use TCP forwarding instead of fighting USB passthrough.
- Run only one adb server: a Windows-side `adb` owning the device will hide it from WSL. Stop the other server (`adb kill-server`) before switching sides.
- Connected inference still depends on board-side `rknn_server` and matching runtime versions.
- ARM64 Toolkit2 on board is different from WSL/x86 Toolkit2: ARM64 supports ONNX/PyTorch conversion and local runtime, not PC-to-board adb connected debugging.
