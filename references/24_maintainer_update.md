# Maintainer: Refresh the Skill for a New RKNN SDK Release

Maintainer-only procedure. **Not an agent task route** for end-user RKNN questions — do not open this on ordinary conversion/debug asks.

## Package scope

| Ship with the skill | Keep out of the package |
|---|---|
| `SKILL.md`, `routing.yaml`, `conventions/`, `workflows/`, `gotchas/` | Local SDK / Model Zoo checkouts (multi-GB) |
| `references/*.md`, `rknn_doc/*.md` | Board binaries, `.so` files, private datasets |
| `scripts/`, `assets/`, `agents/`, `README.md`, `LICENSE` | Build outputs, `__pycache__`, logs |

The installed skill must stay usable with no external checkout. Describe all example paths relative to the user's own Toolkit2 / `rknpu2` / `rknn_model_zoo` install.

## When to bump content

1. **SDK version pin moves** (e.g. V2.3.2 → newer): re-extract or refresh `rknn_doc/` from the official PDFs; update version banners in `rknn_doc/README.md`, `references/00_source_map.md`, and the `SKILL.md` description.
2. **New SoC / runtime lib / API**: update `references/01_platform_matrix.md` first, then the C API matrix in `05`, board paths in `04`, and `gotchas/high-cost.md`.
3. **New Toolkit2 `config()` knobs or renamed quant dtypes**: `references/02_toolkit2_conversion.md` and any workflow that cites them.
4. **New/changed `examples/functions`**: `references/22_onnx_edit_and_graph_tools.md` map; custom-op paths → `21`; sparse → `23`.
5. **Model Zoo FAQ / demo layout**: `references/09_troubleshooting.md`, `references/10_model_zoo_workflows.md`, gotcha #10.

## Recommended update order

```text
1. Diff the new SDK CHANGELOG + doc version tables against packaged rknn_doc/references
2. Patch 01_platform_matrix + gotchas (highest blast radius)
3. Patch 02/03/05/04 for API and board facts
4. Patch 21/22/23 only if functions/custom/sparse changed
5. Refresh rknn_doc/ if PDF text changed (MinerU re-run); keep wildcard-safe names in skill text
6. Sync routing.yaml triggers and the SKILL.md Common Tasks table
7. bash scripts/smoke_test.sh
```

## Must re-verify gates (easy to go stale)

- `sparse_infer` SoC list (RK3576/RV1126B in V2.3.2 API Ref)
- AutoSparsity training “supported SoC” narrative in User Guide vs `config()` table
- Custom-op support matrix rows for mini-runtime
- `quantized_dtype` / `group{N}` / `gdq` legality, the RK2118 `w8a8` exclusion, and the `w4a8` "not implemented" note
- HIDL vs direct-link Android surface (matmul/custom stripped or not)
- Default `target_platform` when unset (RK3566 in V2.3.2) and the exact supported target strings
- First-layer zero-copy width alignment, the int8 vs float16 width×dilation constants, and NC1HWC2 `C2` values per SoC/precision
- Encryption supported-platform list and the `crypt_level`-only API (no password)
- Multi-batch core-count guidance (RK3588 = 3, RK3576 = 2) and zero-copy pairing
- Model Zoo install layout (`install/<soc>_<os>_<arch>/<demo>/`) and the RV110x absolute `LD_LIBRARY_PATH` requirement

## Distillation rules (avoid re-bloating SKILL.md)

- Put numbers and full tables in `references/` or `rknn_doc/`, not `SKILL.md`.
- New procedure → `workflows/`; new landmine → `gotchas/high-cost.md` with **next action**.
- Example paths always relative to the user's SDK install.
- After edits: if a fact is only in `references/` and never linked from a workflow / Common Tasks / gotcha, it is **stored not activated** — fix the route.

## Smoke / hygiene

```bash
bash scripts/smoke_test.sh
```

Must stay green: skill structure, Python/C template syntax, raw-doc availability, single-line frontmatter description, routing targets exist, every reference is linked, Markdown tables are well-formed, and no host-specific absolute paths in skill content.
