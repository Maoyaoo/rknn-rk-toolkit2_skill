# Workflow: rknn_server / connected debug / init failures

## Read first

1. Always Read
2. `references/04_board_debug_rknn_server.md`
3. `references/09_troubleshooting.md`
4. `gotchas/high-cost.md` (version skew, wrong lib)

## Steps

1. Classify failure stage: adb transport · server not running · runtime/lib path · model version · SoC mismatch · memory pressure.
2. Probe:
   - `scripts/rknn_env_probe.py` when Python/host tooling available
   - `adb devices` / `rknn.list_devices()`
   - server + `.so` version strings (paths in `04_board_debug_rknn_server.md`)
   - kernel driver: `dmesg | grep -i rknpu` (want >= 0.9.2)
3. Generate platform-specific push/start commands with `scripts/rknn_server_commands.py`; run them on the host/board. Custom-op connected debug additionally needs `rknn_server` >= 1.6.0.
4. Align Toolkit2 ↔ `rknn_server` ↔ runtime ↔ model `target_platform`.
5. Platform branches:
   - RV110x: `librknnmrt.so`, `/oem/usr/*`, memory/`rkipc`, server ≥ 2.1.0 where required
   - RV1126B: **normal** `librknnrt.so` (not mini-runtime); driver query may use `/proc/rknpu/*`
   - RK2118: no adb/`rknn_server` connected path → `workflows/platform-special.md`
   - WSL: `references/08_wsl.md`
6. Literal log line still unexplained → `workflows/doc-lookup.md` (`MinerU_markdown_02` + `rknn_server_proxy.md`).

## Done when

- Failure class named with at least one concrete check command.
- Version/lib/path matrix addressed if init/load related.
