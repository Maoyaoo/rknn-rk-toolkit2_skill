# Agent Response Conventions (Always Read)

Apply on every RKNN/RKNPU2 task before writing code or commands.

## First moves

1. Identify **target SoC** and **runtime mode** before editing:
   - conversion host vs board runtime
   - Toolkit2 connected inference vs on-board C/Python
   - Android app / RT-Thread when relevant
2. Infer SoC from project files when possible: `rknn.config(target_platform=...)`, CMake toolchain, library paths (`librknnrt` vs `librknnmrt`), board logs. Ask only if it cannot be inferred.
3. Prefer a concrete patch or runnable command sequence. Keep explanation short; state why the Rockchip-specific choice matters.
4. Default user-facing language: **Chinese**, unless the user asked for English.

## Documentation lookup order

1. Distilled: `references/*.md` (and route workflow). For broad asks, open `references/16_task_entry_map.md` if route is unclear.
2. Raw: `rknn_doc/` when you still need a numeric limit, exact API/signature, version gate, error root cause, or per-op constraint. Playbook: `references/15_rknn_doc_lookup.md`.
3. The skill package is self-contained. Do not assume any external SDK or Model Zoo checkout is present; when an example path is needed, describe it relative to the user's own install.
4. If user SDK ≠ V2.3.2, treat `rknn_doc/` numbers as hints; verify against their PDFs/headers for deployment decisions.
5. When quoting a numeric limit, API signature, or version gate, name the source layer (`references/...` or the `rknn_doc/` family) and mark it V2.3.2-scoped when the user's SDK may differ.

## Response contract + self-check

- Shape answers with `references/17_response_contracts.md` (SoC, mode, runtime lib family, version consistency when relevant).
- Before sending, run `references/18_review_checklist.md`.

## Hard platform rules (never skip)

- Wrong API family is a high-cost failure: RV1103/RV1106/B variants and RK2118 use mini-runtime / zero-copy (`rknn_set_io_mem`); do **not** generate general `rknn_inputs_set` / `rknn_outputs_get` as the default path.
- Runtime libs: `librknnrt.so` for RK356x/RK3576/RK3588/RV1126B family; `librknnmrt.so` for RV1103/RV1106 (and B); RK2118 mini runtime comes from RT-Thread SDK, not regular `rknpu2/runtime`.
- Keep Toolkit2, `rknn_server`, runtime library, and model `target_platform` version-aligned.
- Set `target_platform` explicitly; Toolkit2 default is effectively RK3566/RK3568-class only.
- RV1103/RV1106 models are **not** interchangeable with RV1103B/RV1106B models.
- Custom op / `sparse_infer`: full Runtime + SoC gates only (`workflows/graph-advanced.md`). Mini-runtime: no custom-op path. `sparse_infer`: RK3576/RV1126B only.

## Session discipline

- Re-match `routing.yaml` / SKILL.md Common Tasks on **every** new user task in the session.
- Re-read route files only when the route changes or context was compacted.
- Escalate `references/` → `rknn_doc/` before guessing low-level RKNN details.
