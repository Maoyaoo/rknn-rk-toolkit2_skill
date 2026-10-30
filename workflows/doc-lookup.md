# Workflow: Raw doc / exact API or limit lookup

## Read first

1. Always Read
2. `references/15_rknn_doc_lookup.md`
3. `references/00_source_map.md` for file→topic map
4. `rknn_doc/README.md` if file names unclear

## Steps

1. Stay on distilled `references/` until a **specific** gap remains (number, signature, error line, op attr).
2. Pick narrowest `rknn_doc` file from the lookup table; `rg -n` longest distinctive token first.
3. Common maps:
   - ops / limits → `RKNNToolKit2_OP_Support-*.md` then `MinerU_markdown_05_*.md`
   - Toolkit2 Python → `MinerU_markdown_03_*.md`
   - `rknn_convert` YAML / conversion narrative → `MinerU_markdown_02_*.md` §3.1.6
   - RKNNRT C / flags / query → `MinerU_markdown_04_*.md`
   - conversion errors / troubleshooting narrative → `MinerU_markdown_02_*.md`
   - rknn_server → `rknn_server_proxy.md`
   - RGA low-level → `Rockchip_FAQ_RGA_CN.md` / `Rockchip_Developer_Guide_RGA_CN.md`
   - custom op / sparse / onnx_edit narrative → prefer distilled `21`/`22`/`23` first; then `MinerU_markdown_03` config/API + `MinerU_markdown_02` chapters + installed headers
   - Quick Start SoC setup → matching `MinerU_markdown_01_*`
4. Quote the rule with enough context; label as **V2.3.2** when the user's SDK may differ.
5. If the packaged docs still miss, say what must be checked in the user's installed headers/PDFs; do not invent a number.

## Done when

- Gap closed with a citable raw-doc rule, or explicit "not in packaged docs; verify on installed SDK".
