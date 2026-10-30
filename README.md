# rknn-rk-toolkit2 Skill

这是一个给 AI 编码助手使用的 Rockchip RKNN / RKNPU2 技能包。skill 本体包含薄入口 `SKILL.md`、`routing.yaml`、`conventions/`、`workflows/`、`gotchas/`、`references/`、`rknn_doc/`、`scripts/` 和 `assets/`，覆盖模型转换、量化、混合量化、精度分析、Model Zoo 迁移、C/Python 板端推理、`rknn_server` 连板调试、OP 支持检查、性能/内存优化和常见错误排查。

内容主要基于 RKNN SDK V2.3.2 官方文档整理，`rknn_doc/` 内置原文，`references/` 提供蒸馏指导。技能包是自包含的：所有示例路径都以用户自己的 Toolkit2 / `rknpu2` / `rknn_model_zoo` 安装目录为基准。

## 安装

安装目录名需与 `SKILL.md` 中的 `name` 一致：`rknn-rk-toolkit2`。下面假定你已经拿到 skill 包目录（本仓库根目录，复制时重命名为 `rknn-rk-toolkit2`）。

### Qoder

用户级（所有项目可用）：

```bash
mkdir -p ~/.qoder-cn/skills
cp -r rknn-rk-toolkit2_skill ~/.qoder-cn/skills/rknn-rk-toolkit2
# 期望路径：~/.qoder-cn/skills/rknn-rk-toolkit2/SKILL.md
```

项目级（仅当前项目）：

```bash
mkdir -p .qoder/skills
cp -r rknn-rk-toolkit2_skill .qoder/skills/rknn-rk-toolkit2
# 期望路径：.qoder/skills/rknn-rk-toolkit2/SKILL.md
```

安装后重启会话或执行 `/skills reload`，用 `/skills list` 确认已加载，再用 `/rknn-rk-toolkit2` 或直接描述 RKNN 任务触发。

### ZCode（智谱 Z.ai）

ZCode 直接读取 `SKILL.md` 的 `name` / `description`，不需要额外的 manifest 或 `config.json`；安装目录名需与 `name` 一致。

用户级（所有项目可用）：

```bash
mkdir -p ~/.zcode/skills
cp -r rknn-rk-toolkit2_skill ~/.zcode/skills/rknn-rk-toolkit2
# 期望路径：~/.zcode/skills/rknn-rk-toolkit2/SKILL.md
```

工作区级（仅当前项目）：

```bash
mkdir -p .zcode/skills
cp -r rknn-rk-toolkit2_skill .zcode/skills/rknn-rk-toolkit2
# 期望路径：.zcode/skills/rknn-rk-toolkit2/SKILL.md
```

安装后在 ZCode 的“设置 → 技能”中确认已启用；对话中直接描述 RKNN 任务，或点名 `rknn-rk-toolkit2` 触发。ZCode 对 `name` 的要求（小写、无空格/点、≤64 字符）和 `description` ≤1024 字符，本包均已满足。

### Claude Code

用户级（所有项目可用）：

```bash
mkdir -p ~/.claude/skills
cp -r rknn-rk-toolkit2 ~/.claude/skills/
# 期望路径：~/.claude/skills/rknn-rk-toolkit2/SKILL.md
```

项目级（仅当前项目）：

```bash
mkdir -p .claude/skills
cp -r rknn-rk-toolkit2 .claude/skills/
# 期望路径：.claude/skills/rknn-rk-toolkit2/SKILL.md
```

在 Claude Code 中通过 Skill 工具调用，名字为 `rknn-rk-toolkit2`。

### Codex（OpenAI）

Codex 把含 `SKILL.md` 的文件夹识别为技能，支持 `$rknn-rk-toolkit2` 显式调用，也会按 `description` 隐式匹配。安装目录名需与 `name` 一致，且不能含空格或中文。

用户级（所有项目可用）：

```bash
mkdir -p ~/.codex/skills
cp -r rknn-rk-toolkit2_skill ~/.codex/skills/rknn-rk-toolkit2
# 期望路径：~/.codex/skills/rknn-rk-toolkit2/SKILL.md
```

项目级（仅当前项目）：

```bash
mkdir -p .codex/skills
cp -r rknn-rk-toolkit2_skill .codex/skills/rknn-rk-toolkit2
# 期望路径：.codex/skills/rknn-rk-toolkit2/SKILL.md
```

若设置了 `CODEX_HOME`，用户级目录相应为 `$CODEX_HOME/skills/rknn-rk-toolkit2`。也可以在 `~/.codex/config.toml` 中用 `[skills]` 指定额外技能目录：

```toml
[skills]
enabled = true
directory = "~/codex-skills"
```

不同 Codex 版本的扫描范围略有差异：除 `~/.codex/skills` 外，部分版本还读取跨工具的 `~/.agents/skills`（项目内 `.agents/skills`）和系统级 `/etc/codex/skills`。若安装后没有被识别，把同一目录复制或软链到 `~/.agents/skills/rknn-rk-toolkit2` 再试。

本包自带 `agents/openai.yaml`（展示名、默认提示词、`allow_implicit_invocation`），Codex 的技能界面会读取它；缺少该文件也能正常工作。

安装后重启 Codex 进程（CLI 退出重进、IDE 重载窗口），在 `/skills` 列表确认已加载，然后显式调用：

```text
$rknn-rk-toolkit2 把 export.onnx 转成 RK3576 的 rknn，做 w8a8 量化
```

Codex 还会读取项目根目录的 `AGENTS.md`，可配合下文「集成到项目 AGENTS.md」使用。如果你拿到的是 zip 包，把上面的 `cp -r` 换成 `unzip rknn-rk-toolkit2.zip -d` 即可。

## 推荐触发词

```text
$rknn-rk-toolkit2 帮我把这个 ONNX 转成 RK3588 可运行的 rknn，并补充 C API 推理代码。
```

```text
$rknn-rk-toolkit2 分析这个 rknn_init 返回 RKNN_ERR_DEVICE_UNAVAILABLE 的原因。
```

```text
$rknn-rk-toolkit2 这个 Softmax 在 RK3576 上支持吗？有什么属性限制？
```

```text
$rknn-rk-toolkit2 在 RV1106 上跑这个模型，rknn_server 一直起不来，帮我排查。
```

```text
$rknn-rk-toolkit2 把 RK3588 的模型加密导出，并给出板端加载注意事项。
```

## 集成到项目 AGENTS.md

把 `assets/AGENTS_SNIPPET.md` 的内容追加到项目根目录的 `AGENTS.md`，可提醒 agent 在处理 RKNN 代码时启用本 skill，并先确认平台矩阵与版本一致性。

## 目录

```text
rknn-rk-toolkit2/
├── SKILL.md                 # 薄入口：Always Read + Common Tasks（≤预算）
├── routing.yaml             # 任务路由源（与 SKILL.md Common Tasks 对齐）
├── conventions/             # Always-read 行为约定
├── workflows/               # 按任务类型的步骤流
├── gotchas/                 # 高成本坑（激活层）
├── references/              # 蒸馏指导（~80% 默认查这里）
├── rknn_doc/                # V2.3.2 原文 markdown（权威 per-op / API）
├── scripts/
├── assets/
├── agents/openai.yaml
├── README.md
└── LICENSE
```

发布时只打包以上目录和文件；不要带入本地 SDK checkout、板端二进制、私有数据集或构建产物。

**结构原则**（skill-based-architecture）：

| 层 | 位置 | 角色 |
|---|---|---|
| 入口路由 | `SKILL.md` + `routing.yaml` | 触发、Always Read、任务分派 |
| 过程骨架 | `workflows/` | 各任务步骤 |
| 家规 | `conventions/` | 每次任务都适用的回答约定 |
| 高成本坑 | `gotchas/` | 可改变下一步动作的 landmine |
| 蒸馏肉 | `references/*.md` | 跨章节稳定模式 |
| 原文肉 | `rknn_doc/*.md` | V2.3.2 数字 / 签名 / per-op 限制 |

## Agent 查阅顺序

1. 读 Always Read：`conventions/agent-response.md` + `references/01_platform_matrix.md`
2. 按 `SKILL.md` Common Tasks / `routing.yaml` 打开对应 `workflows/*` 与 required reads
3. 缺具体数字、API 签名、报错根因、op 限制时 → `references/15_rknn_doc_lookup.md` → `rknn_doc/`
4. 只有用户 SDK 明显不是 V2.3.2，或 skill 两层都盖不住时，才回退到用户本地 SDK/PDF/头文件

skill 包自包含，不依赖任何外部 SDK checkout；示例路径以用户自己的 SDK 安装目录为准。

常用入口：

- 任务总表：`references/16_task_entry_map.md`
- 原文检索：`references/15_rknn_doc_lookup.md`
- 回答合同 / 自检：`references/17_response_contracts.md`、`18_review_checklist.md`
- 高成本坑：`gotchas/high-cost.md`
- 高级特性（加密 / 多 batch / 权重共享 / NPU SRAM / QAT）：`references/13_advanced_features.md`
- 官方最小验证路径：`references/20_official_examples_and_smoke_paths.md`
- 自定义算子 / onnx_edit / 稀疏：`references/21_custom_op.md`、`22_onnx_edit_and_graph_tools.md`、`23_sparsity_and_toolkit_functions.md`
- 维护者按新 SDK 版本刷新 skill：`references/24_maintainer_update.md`（非终端用户任务路由）

```bash
rg -n "Softmax|ConvTranspose|RMSNorm" rknn_doc/RKNNToolKit2_OP_Support-*.md
rg -n "accuracy_analysis|dynamic_input|op_target|codegen" rknn_doc/MinerU_markdown_03_*.md
rg -n "rknn_init_extend|rknn_create_mem2|RKNN_QUERY_" rknn_doc/MinerU_markdown_04_*.md
rg -n "RKNN_ERR_DEVICE_UNAVAILABLE|Invalid RKNN model version|NonMaxSuppression" \
  rknn_doc/MinerU_markdown_02_*.md rknn_doc/rknn_server_proxy.md
```

**版本提示**：`rknn_doc/` 固化在 V2.3.2（2025-04-03）。用户 SDK 不一致时视为提示，以实际 PDF/头文件为准。

## 校验

```bash
bash scripts/smoke_test.sh
```

这会对所有 Python 脚本运行 `--help` 与 `py_compile`，对所有 C/C++ 模板用极简 RKNN/RGA stub 做 `gcc -fsyntax-only` 语法检查，检查 reference 文件非空、`SKILL.md` frontmatter（`name` 匹配、符合 Qoder/ZCode/Claude Code/Codex 的小写连字符且 ≤64 字符、`description` 单行且不超过 1024 字符）、skill 内容中的主机绝对路径、`routing.yaml` / `SKILL.md` 指向的文件是否存在、每个 reference 是否从入口层可达、Markdown 表格列数是否一致，以及 `rknn_doc/` 原文是否完整，作为一次廉价的回归检查。

## 注意

遇到 SDK 版本不一致、头文件与文档冲突、板端 BSP 定制修改时，以实际 SDK 包中的头文件、库版本和板端日志为准。
