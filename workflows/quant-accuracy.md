# Workflow: Quantization / accuracy triage

## Read first

1. Always Read
2. `references/03_quantization_accuracy.md`
3. `references/02_toolkit2_conversion.md`
4. On wrong boxes/classes after Model Zoo port: also `references/10_model_zoo_workflows.md`

## Steps

1. Separate **preprocess/postprocess mismatch** from true **quantization loss**.
2. Comparison order (do not skip):
   - original framework → Toolkit2 FP simulator → quant simulator → board runtime
3. If quant path is bad:
   - rebuild calibration set (count + domain); multi-input format if needed
   - run `assets/accuracy_analysis_template.py` (or Toolkit2 `accuracy_analysis`)
4. Escalate knobs only after analysis: algorithm (`normal`/`mmse`/`kl`/`gdq`; `gdq` only with `w4a16`/`w8a16`), method (`channel`/`group{N}`; `group` only with `w4a16`), `auto_hybrid` (w8a8 builds only), manual hybrid step1/2.
5. Platform gates: e.g. `w4a16` / group on allowed SoCs only — verify in platform + conversion refs before recommending (`w8a8` off on RK2118, `w4a8` unimplemented).
6. QAT last; only after hybrid/auto_hybrid insufficient. Advanced entry: `references/13_advanced_features.md`.
7. Do **not** treat `sparse_infer` / AutoSparsity as an accuracy fix — that is a structure/perf path (`workflows/graph-advanced.md`).

## Done when

- Root cause class stated (data / calibration / graph / postprocess).
- Next concrete command or config change given; QAT not jumped to first.
