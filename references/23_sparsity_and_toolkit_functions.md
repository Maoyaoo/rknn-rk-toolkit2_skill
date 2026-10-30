# Sparsity, Pruning, and Sparse Inference

Covers training-time **AutoSparsity**, Toolkit2 **`sparse_infer` / `model_pruning`**, and how they relate. Distilled from the SDK `autosparsity/` package READMEs, Toolkit2 `model_pruning` example, User Guide sparse chapters, and config tables in `references/02_toolkit2_conversion.md`.

Package paths (`autosparsity/`, `examples/functions/model_pruning/`) refer to the **user's RKNN-Toolkit2 SDK tree**, not this skill.

## Concepts (do not mix)

| Term | When | What it does |
|---|---|---|
| **AutoSparsity (training)** | During PyTorch training (CUDA) | Forces structured sparse weights via `autosparsity.sparsity.sparsity_model` |
| **`sparse_infer=True`** | Toolkit2 `rknn.config` at convert time | Builds/runs sparse inference path for a **pre-sparsified** model |
| **`model_pruning=True`** | Toolkit2 `rknn.config` | Lossless pruning when weights are already sparse — shrinks `.rknn` / compute |
| **Structured sparsity hardware path** | Board Runtime | Speedup only on SoCs that implement sparse infer |

## Platform gates (V2.3.2, from Toolkit2 API Ref + User Guide)

| Feature | Official gate | Source |
|---|---|---|
| `config(sparse_infer=True)` | **RK3576 / RV1126B only** | Toolkit2 API Ref `config()` table: “目前仅对RK3576/RV1126B生效” |
| AutoSparsity **training package** + UG sparse chapter examples | Documented / demoed primarily on **RK3576** | User Guide sparse chapter: “目前该功能仅支持RK3576”; SDK `autosparsity` example notes |
| `config(model_pruning=True)` | No separate SoC whitelist in the API table; useful only when weights are already sparse | Toolkit2 API Ref; UG § model pruning |
| `single_core_mode` (related size knob, not sparsity) | **RK3588 / RK3576 only** | Toolkit2 API Ref |

Rules for agents:

- Do **not** enable `sparse_infer` on RK3588 / RK356x / RV110x / RK2118 expecting speedup.
- For RV1126B: `sparse_infer` is allowed by the Toolkit2 config table, but AutoSparsity **training** docs still center on RK3576 — train/export carefully and verify on the user's SDK/board before promising wins.
- `model_pruning` does **not** create sparsity; it losslessly drops zeros/channels on already-sparse weights. Dense models: skip.
- If user SDK ≠ V2.3.2, re-check the installed `config()` table.

## AutoSparsity training flow

1. Install the wheel from the SDK package tree (name/version may change per release), e.g.  
   `pip install packages/autosparsity-*-linux_x86_64.whl` under the SDK `autosparsity/` dir.
2. CUDA required for the reference training path.
3. Before the training loop:

```python
from autosparsity.sparsity import sparsity_model

model = ...  # torch nn.Module on CUDA
optimizer = ...
mode = 0     # follow installed autosparsity docs for mode meanings
sparsity_model(model, optimizer, mode)

# normal training loop
for ...:
    loss = ...
    loss.backward()
    optimizer.step()
```

4. Export the trained model to ONNX/Torch as usual for Toolkit2 (`load_onnx` / `load_pytorch`).
5. Convert with sparse-aware config (next section).

## Toolkit2 conversion knobs

```python
rknn.config(
    target_platform='rk3576',   # or 'rv1126b' for sparse_infer only if that SoC is the target
    sparse_infer=True,          # RK3576 / RV1126B only (V2.3.2 API Ref)
    model_pruning=True,         # optional: lossless prune of already-sparse weights
    # ... mean/std etc.
)
rknn.load_onnx(...)  # or load_pytorch
rknn.build(do_quantization=True, dataset='./dataset.txt')
rknn.export_rknn('sparse.rknn')
```

Board C deploy after a sparse-enabled export: use the normal general-API or zero-copy flow — no separate “sparse C API”. The sparse schedule is baked into the `.rknn` at convert time.

Validation:

- First prove **dense** baseline accuracy/perf on the same SoC.
- Then compare sparse model TOP-k / task metric and `eval_perf`.
- If accuracy collapses, sparsity was too aggressive or applied to wrong layers — fix training, do not “fix” with random quant knobs first.

## Relation to other advanced features

- **FlashAttention / `enable_flash_attention`**: orthogonal attention optimization (`02_toolkit2_conversion.md`); not a substitute for weight sparsity.
- **Multi-batch / multi-core**: can combine with sparse models only if the SoC supports both; still follow core-count rules (`13_advanced_features.md`).
- **QAT / hybrid quant**: sparsity is a **weight structure** change; still run normal accuracy triage after convert (`03_quantization_accuracy.md`).

## `model_pruning` example path

Toolkit2 `examples/functions/model_pruning/` demonstrates size/compute reduction on already-sparse weights. Use it when:

- weights show large zero ratios after sparse training or structured prune, and  
- target supports the prune path in that SDK version.

If the model is dense, skip — pruning config will not magically create sparsity.

## Decision tree

```text
Need smaller/faster model?
  ├─ Target RK3576 and can retrain with CUDA? → AutoSparsity + sparse_infer (+ optional model_pruning)
  ├─ Target RV1126B, already have sparsified weights? → sparse_infer (+ model_pruning if beneficial); do not assume AutoSparsity train docs cover RV1126B
  ├─ Already sparse weights on other SoCs? → try model_pruning only; sparse_infer will not apply
  └─ Dense + no retrain → hybrid quant, onnx_edit, op rewrite, core/batch — not sparse_infer
```

## Pitfalls

- Setting `sparse_infer=True` on an ordinary dense ImageNet export — wrong path; no magic.
- Training sparsity without CUDA / wrong wheel Python ABI.
- Expecting RV110x memory wins from sparse_infer — wrong SoC family.
- Comparing sparse vs dense without fixed frequency (`scaling_frequency`) — noise dominates.
- Confusing **lossless** `model_pruning` of sparse tensors with unstructured magnitude prune that needs fine-tune.

## Escalation

- Exact sparse op coverage / SoC columns → `rknn_doc/MinerU_markdown_05_*.md` + User Guide sparse chapter.
- Config parameter legality → `rknn_doc/MinerU_markdown_03_*.md` `config()` table.
