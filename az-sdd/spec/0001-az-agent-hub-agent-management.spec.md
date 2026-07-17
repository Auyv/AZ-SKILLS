# [0001] az-agent-hub Agents 组织与管理

## 基本信息

| 字段 | 内容 |
|------|------|
| 功能编号 | 0001 |
| 功能名称 | az-agent-hub Agents 组织与管理 |
| 所属模块 | az-agent-hub |
| 规范模式 | strict |
| 状态 | 草稿 |
| 创建日期 | 2026-07-15 |
| 最后更新 | 2026-07-15 |
| 需求来源 | `C:\Users\jpz\Desktop\az-agent-hub.txt` |

## 背景与定位

AZ-SKILLS 当前以 `skills/` 作为 Skill 的单一真相来源，并通过 `manifest.yaml` 和 `sync_skills.py` 分发到多个 AI 编程工具。仓库尚缺少同等级的 Agent 组织能力：Agent prompt、规则组合、技能组合和目标工具格式之间没有统一声明、编译、漂移检查与同步入口。

本功能新增命令行工具 `az-agent-hub`。用户只维护 `agents/<name>.agent.yaml`，工具负责校验、继承展开、规则与技能解析、跨工具编译、漂移检查和显式同步。

## 核心原则

1. `agents/<name>.agent.yaml` 是 Agent 定义的唯一权威源。
2. 读取、校验和漂移检查不得修改任何文件。
3. `init`、`build`、`sync`、`rule init` 和 `draft import` 等写操作必须由用户显式执行。
4. `sync` 不得覆盖无法确认由 az-agent-hub 管理的文件。
5. target 差异封装在 renderer 中，不得污染 Agent 的通用声明格式。
6. 与当前 `skills/`、`rules/` 和 `manifest.yaml` 共存，不破坏既有 Skill 同步流程。

## 用户与使用场景

### 目标用户

- AZ-SKILLS 仓库维护者。
- 需要让同一 Agent 在 Codex、OpenCode 等工具中复用的开发者。
- 需要检查生成产物与工作区是否漂移的维护人员。

### 主要场景

1. 初始化 Agent 管理目录和示例文件。
2. 编写基础 Agent，并通过 `extends` 派生专业 Agent。
3. 给 Agent 组合规则和现有 AZ-SKILLS Skill。
4. 在提交或同步前校验所有引用和继承关系。
5. 将 Agent 编译为多个 target 的产物。
6. 只读检查 `dist/` 或目标工作区与源声明是否漂移。
7. 用户确认后手动同步到目标 AI 工具工作区。
8. 从既有 target 文件反向生成 `drafts/` 草稿，人工审核后再手动纳入 `agents/`。

## 范围

### v1.0 必须交付

- 项目目录初始化。
- Agent YAML 校验、单继承、继承展开和来源追踪。
- 结构化 rules 管理、引用、禁用和覆盖。
- skills 引用、继承追加及当前 AZ-SKILLS Skill 兼容。
- `raw-prompt`、`codex`、`opencode` 三个 target renderer。
- 可扩展 renderer 接口，为 `hermes`、`claude-code` 预留 target 名称和目录。
- 确定性编译、漂移检查、显式同步和反向草稿。
- 可供外部计划任务调用的稳定退出码；v1.0 不自行安装或管理定时任务。

### v1.0 不包含

- 图形界面或 Web 服务。
- Agent 多继承。
- Skill 的 disable/override 机制。
- 自动修改 `agents/`、现有 `skills/`、现有散装规则文件或目标工作区。
- 自动下载模型、Skill、Rule 或 target 插件。
- Hermes、Claude Code renderer 的正式交付；其实现可在后续规范中补充。
- 云端账户、多人权限和远程数据库。

## 项目目录

显式执行初始化后，在项目根目录创建：

```text
agents/                       # Agent 声明，唯一真相源
├── base.agent.yaml
└── mcu-dev.agent.yaml
rules/                        # 既有规则目录；结构化规则使用 rules/<id>/
skills/                       # 复用当前 Skill 单一真相源
dist/                         # 生成产物，不作为输入源
├── codex/
├── opencode/
├── hermes/
├── claude-code/
└── raw-prompt/
drafts/                       # 反向吸收草稿，不自动进入 agents/
az-agent-hub.config.yaml      # Agent Hub 配置
examples/                     # 示例与 target fixture
```

初始化必须幂等：默认只创建缺失目录和缺失示例，不覆盖任何已有文件。需要重建示例时必须使用明确的覆盖参数，并逐项报告将受影响的文件。

## Agent 声明格式

文件路径：`agents/<name>.agent.yaml`。

### 字段

| 字段 | 必填 | 类型 | 规则 |
|------|------|------|------|
| `name` | 是 | string | 匹配 `^[a-z0-9-]+$`，并与文件名主体一致 |
| `version` | 是 | string | 符合 SemVer `x.y.z` |
| `prompt` | 是 | multiline string | 展开后的最终值必须非空 |
| `description` | 否 | string | 单行说明 |
| `extends` | 否 | string | 相对当前 Agent 文件或项目根目录的 YAML 路径 |
| `rules` | 否 | list | Rule 路径、Rule id 或带覆盖字段的对象 |
| `skills` | 否 | list | Skill 路径、Skill name 或带附加 tags 的对象 |
| `model` | 否 | string | target 支持时输出；不支持时给出明确诊断 |
| `temperature` | 否 | number | 闭区间 `[0, 2]` |
| `max_tokens` | 否 | integer | 大于 0 |
| `changelog` | 否 | list | 每项至少包含 `version` 和非空描述 |
| `disable_rules` | 否 | list | 仅引用父链已存在的 Rule id |
| `override_rules` | 否 | map | key 为父 Rule id，value 为新 Rule 路径或 id |
| `tags` | 否 | list | 字符串列表，合并后去重 |

字段名统一使用 `disable_rules` 和 `override_rules`；不接受无下划线别名，避免出现两套格式。

### 继承规则

- 只允许单继承。
- 从当前 Agent 到最顶层父 Agent 的边数不得超过 3。
- 禁止直接或间接循环引用。
- `prompt`、`model`、`temperature`、`max_tokens`、`version`、`changelog`：子值覆盖父值。
- `rules`、`skills`：父项在前、子项在后，按规范化 id/name 去重。
- `tags`：父子合并去重，并保持首次出现顺序。
- `disable_rules`：在完整父链展开后移除对应 Rule。
- `override_rules`：在完整父链展开后用新 Rule 完整替换父 Rule，保留列表位置并更新来源标记。
- `name` 和 `description` 不继承；它们描述当前 Agent。
- 每个展开字段和 Rule/Skill 条目均保留来源 Agent，供 `show --resolved` 和诊断输出使用。

### 校验

- 必填字段、类型、name 正则、文件名一致性和 SemVer 均须校验。
- `extends`、Rule 和 Skill 路径必须存在，解析后的路径不得逃逸项目根目录。
- 同一 Agent 的 Rule 对象中若出现 `id`，规范化后不得重复。
- `disable_rules`、`override_rules` 必须引用父链中存在的 id。
- YAML 未知字段默认报错；只有配置显式允许兼容模式时才降级为警告。
- 一次校验尽量汇总全部独立错误，输出文件、字段路径、原因和修复建议。

## Rules 系统

### 结构化 Rule

```text
rules/<rule-id>/
├── rule.yaml
└── rule.md
```

`rule.yaml` 字段：

| 字段 | 必填 | 规则 |
|------|------|------|
| `id` | 是 | 与目录名一致，匹配 `^[a-z0-9-]+$` |
| `severity` | 否 | `must`、`should`、`may`，默认 `must` |
| `name` | 否 | 人类可读名称 |
| `version` | 否 | SemVer |
| `description` | 否 | 单行说明 |
| `tags` | 否 | 字符串列表 |
| `changelog` | 否 | 版本变更列表 |

`rule.md` 必须存在且正文非空，应能容纳规则说明、示例和反例。

当前仓库 `rules/` 下已有的散装 `.md` 规则不自动迁移、不删除，也不被 `rule list` 当作结构化 Rule；用户可通过 `rule init` 显式创建包装目录。

### Rule 引用

以下形式等价地解析为规范化 Rule：

```yaml
rules:
  - rules/r-c99-style
  - r-no-malloc
  - id: r-freertos-no-delay
    severity: should
```

- 字符串包含路径分隔符时按路径解析，否则从配置的全局 rules 目录按 id 查找。
- 引用对象可覆盖本次引用的 `severity`，但不得修改源 `rule.yaml`。
- 解析后按 `must -> should -> may` 分组，同组内保持继承展开后的稳定顺序。

### Rule 命令

| 命令 | 行为 |
|------|------|
| `rule init <rule-id>` | 显式创建不覆盖已有文件的 Rule 脚手架 |
| `rule list` | 列出全部结构化 Rule、版本、severity 和路径 |
| `rule validate <rule-id-or-path>` | 只读校验单条 Rule |
| `rule show <rule-id>` | 显示元信息和 Markdown 正文 |
| `rule for-agent <agent>` | 显示 Agent 展开后的规则、severity 和来源 |

## Skills 系统

### 原生结构

```text
skills/<skill-name>/
├── skill.yaml
└── SKILL.md
```

`skill.yaml` 必填 `name`、`version`，选填 `description`、`tags`、`changelog`；`SKILL.md` 必须存在且非空。

### AZ-SKILLS 兼容规则

为避免要求当前仓库所有 Skill 重复维护元数据，v1.0 同时接受只有 `SKILL.md` 的目录，前提是其 YAML front matter 含 `name`、`version`、`description`。

- 同时存在 `skill.yaml` 和 `SKILL.md` 时，`name`、`version` 必须一致，否则校验失败。
- 只有 `SKILL.md` 时，从 front matter 读取元数据。
- 只有 `skill.yaml`、缺少 `SKILL.md` 时校验失败。
- 读取和编译不得修改 Skill 源文件。
- `skills` 继承只追加并按规范化 name 去重，v1.0 不提供 disable/override。

支持以下引用：

```yaml
skills:
  - skills/code-search
  - code-search
  - id: arm-cortex-debug
    tags: [debug, arm]
```

对象中的 `tags` 只附加到该 Agent 的展开结果，不回写 Skill 元数据。

## Target 与编译

### v1.0 target

| target | 状态 | 输出要求 |
|--------|------|----------|
| `raw-prompt` | 必须实现 | 纯文本/Markdown，包含 prompt、按 severity 分组的完整 Rule 正文、Skill 列表和生成标记 |
| `codex` | 必须实现 | 生成 Codex custom agent TOML，并通过仓库内 fixture/validator 验证 |
| `opencode` | 必须实现 | 由独立 renderer 按实现时 OpenCode 可加载契约生成，并通过仓库内 fixture/validator 验证 |
| `hermes` | 预留 | v1.0 返回明确的“不支持”诊断，不生成伪产物 |
| `claude-code` | 预留 | v1.0 返回明确的“不支持”诊断，不生成伪产物 |

### Codex renderer 契约

- dist 产物为 `dist/codex/<name>.toml`，显式同步的项目级目标为 `.codex/agents/<name>.toml`；个人级目标路径由配置显式选择。
- TOML 必填字段映射：Agent `name` -> `name`，Agent `description` -> `description`，展开后的 prompt 与 Rule 指令 -> `developer_instructions`。
- 构建 Codex target 时，源 Agent 的 `description` 必须非空；缺失时 target 校验失败，不自动编造说明。
- Agent `model` 映射为 Codex `model`；其他字段仅在当前 Codex 配置契约支持时映射。
- skills 优先映射到 Codex 支持的 `skills.config`；若无法无损映射，则将稳定的 Skill 名称与使用说明编入 `developer_instructions`，并给出显式警告。
- `temperature`、`max_tokens` 或其他 Codex 不支持的字段不得静默写入；renderer 按配置选择“警告并忽略”或“严格失败”，strict 模式默认失败。
- 实现时必须把当时官方 custom agent 样例和必填字段固化为 fixture，并记录验证所用 Codex 版本，防止后续格式演进造成无提示漂移。

### 生成产物约束

- 默认写入 `dist/<target>/`，不得直接写目标工具工作区。
- 每个产物包含生成标记、源 Agent 相对路径、源版本和内容摘要。
- `raw-prompt` 顶部包含 `<!-- Generated by az-agent-hub from <source> at <time> -->`。
- 同一输入在忽略生成时间元数据后必须得到相同正文和相同内容摘要。
- 漂移比较使用规范化内容摘要，不得因时间戳变化产生假漂移。
- 构建前完成完整校验；任一必需引用失败时不留下半成品。
- 多 Agent 或多 target 构建采用临时目录完成，全部成功后再原子替换本次受管产物。
- 不支持的 Agent 字段不得被静默丢弃；renderer 必须输出警告或错误策略。

## 漂移检查与同步

### 漂移状态

| 状态 | 含义 |
|------|------|
| `clean` | 目标受管内容与当前源展开结果一致 |
| `drift` | 目标受管内容存在但摘要不一致 |
| `missing` | 期望的目标受管文件不存在 |
| `unmanaged` | 目标位置存在文件，但没有合法生成标记或受管清单记录 |
| `invalid` | 源、配置或目标产物无法解析 |

`check`/`drift` 命令只读，可供 Windows 任务计划程序或其他外部调度器定时调用。退出码固定为：`0=clean`、`2=发现 drift/missing`、`3=校验或配置错误`、`4=存在 unmanaged 冲突`。

### 显式同步

- 只有用户执行 `sync` 才可写目标工具工作区。
- `sync --dry-run` 只输出计划，不写文件。
- 同步前必须先校验并构建候选产物。
- 默认仅创建或更新带合法生成标记且属于当前 Agent/target 的文件。
- 遇到 unmanaged 同名文件时停止并提示，不得覆盖。
- 删除过期受管文件必须使用显式 `--prune`；无该参数不得删除。
- 同步失败时保留原目标文件，并返回非零退出码。
- 工具不得修改 `manifest.yaml` 的 `last_sync`，避免与 `sync_skills.py` 的 Skill 同步状态混淆。

## 反向草稿

- `draft import --target <target> --source <path>` 是显式写操作。
- 输出写入 `drafts/<target>/<name>.agent.yaml`，并标记来源、导入时间和“未审核草稿”。
- 不得直接创建或覆盖 `agents/*.agent.yaml`。
- 无法无损映射的 target 字段写入诊断区，不能静默丢失。
- 从草稿进入 `agents/` 由用户人工审核和移动，不属于自动流程。

## CLI 行为

建议的用户命令面如下，具体 Python 包结构在技术规范阶段定版：

```text
az-agent-hub init [--root PATH]
az-agent-hub agent list
az-agent-hub agent show <name> [--resolved]
az-agent-hub agent validate [<name>|--all]
az-agent-hub build <name>|--all --target <target>[,<target>...]
az-agent-hub drift <name>|--all --target <target>[,<target>...]
az-agent-hub sync <name>|--all --target <target>[,<target>...] [--dry-run] [--prune]
az-agent-hub rule init|list|validate|show|for-agent ...
az-agent-hub skill list|validate|show ...
az-agent-hub draft import --target <target> --source <path>
```

所有命令支持 `--root` 或从当前目录向上查找 `az-agent-hub.config.yaml`。错误输出必须包含失败对象、原因和修复建议；批量命令输出成功、警告、失败汇总。

## 配置与当前项目集成

`az-agent-hub.config.yaml` 至少包含：

- Agent、Rule、Skill、dist、drafts 根目录。
- 启用的 target 及 renderer 配置。
- target 工作区路径。
- 是否从当前 `manifest.yaml` 的 `agent_targets` 只读继承 target 名称和基础路径。
- 未知字段策略和路径解析策略。

集成约束：

1. `manifest.yaml` 仍由现有 Skill 同步流程维护，Agent Hub 默认只读。
2. `skills/` 仍是 Skill 单一真相来源，不复制到新的 Agent 专用 Skill 目录。
3. 现有 `rules/*.md` 文件保留；Agent Hub 只管理符合目录结构的 Rule。
4. Agent 产物默认进入 `dist/`，不得混入 `skills/`。
5. target 工作区路径可复用 `manifest.yaml` 的平台路径，但 Agent 子路径和文件契约由 renderer 决定。

## 技术约束

### 技术栈

| 项目 | 约束 |
|------|------|
| 语言 | Python 3，与现有 `sync_skills.py` 运行环境一致 |
| YAML | 优先复用 PyYAML；缺少依赖时给出明确提示，不自动安装 |
| CLI | 标准库 `argparse` 或等价方案，不引入 Web 服务 |
| 路径 | 使用 `pathlib`，兼容 Windows 路径和 `~` 展开 |
| 编码 | 所有文本文件使用 UTF-8 |
| 写入 | 临时文件 + 原子替换；只写显式命令授权的范围 |
| 日志 | 默认简洁文本，支持机器可读输出供自动检查使用 |

### 建议模块边界

- schema/validator：Agent、Rule、Skill 和配置校验。
- resolver：继承、引用、disable/override、来源追踪。
- renderers：每个 target 一个独立适配器。
- drift：规范化摘要、状态判定和退出码。
- sync：受管文件清单、dry-run、原子写入和 prune。
- drafts：target 反向映射为草稿。
- cli：参数解析和结果展示。

### 安全与可维护性

- 所有源引用和输出路径在解析后校验边界，禁止 `..` 或链接逃逸根目录。
- 不执行 Agent、Rule 或 Skill 文本中的命令和代码。
- 生成标记不能作为唯一信任依据；同时校验受管清单和目标相对路径。
- 诊断中避免输出环境变量、凭据和目标文件中的敏感内容。
- renderer 采用注册表/协议扩展，新增 target 不修改 resolver 核心逻辑。

## 输入项

| 输入 | 来源 | 必填 |
|------|------|------|
| Agent YAML | `agents/*.agent.yaml` | 是 |
| Rule 元信息与正文 | `rules/<id>/rule.yaml`、`rule.md` | Agent 引用时必填 |
| Skill 元信息与正文 | `skills/<name>/skill.yaml`、`SKILL.md` 或兼容 front matter | Agent 引用时必填 |
| Hub 配置 | `az-agent-hub.config.yaml` | 除初始化外必填 |
| target 与 Agent 选择 | CLI 参数或配置默认值 | 构建、检查、同步时必填 |

## 输出项

| 输出 | 位置/形式 |
|------|-----------|
| 校验与展开结果 | 终端文本或机器可读结果 |
| 编译产物 | `dist/<target>/` |
| 漂移报告 | 终端或显式指定的报告文件 |
| 同步计划 | `sync --dry-run` 终端输出 |
| 受管文件清单 | `dist/` 内部元数据，不污染源目录 |
| 反向草稿 | `drafts/<target>/` |

## 异常与边界场景

- 空项目、无 Agent、无 Rule/Skill 引用时给出可操作提示。
- YAML 语法错误、编码错误、未知字段、重复 id/name 分别定位到具体文件和字段。
- 父文件缺失、循环继承、深度超限、路径逃逸必须阻止构建。
- Rule/Skill 重名或 id/name 与目录不一致必须阻止歧义解析。
- target 未支持、target 工作区不存在、权限不足时不得留下部分写入。
- unmanaged 文件冲突必须停止同步。
- 生成时间变化不得单独造成 drift。
- Windows 路径大小写、分隔符和 `~` 展开后使用规范化路径比较。

## 验收标准

- [ ] [001] 显式执行 `init` 后创建要求的目录、配置和示例，重复执行不覆盖已有文件。
- [ ] [002] 缺少 `name`、`version` 或 `prompt`，或 name/SemVer/数值范围不合法时，校验失败并定位字段。
- [ ] [003] 单继承按规定覆盖和追加字段，展开结果保留每项来源。
- [ ] [004] 循环继承、父文件缺失或继承深度超过 3 时，构建被阻止并给出继承链。
- [ ] [005] `disable_rules` 和 `override_rules` 正确作用于父链；引用不存在的父 Rule id 时校验失败。
- [ ] [006] Rule 可按路径、id 和带 severity 覆盖的对象三种方式解析，最终按 must、should、may 分组。
- [ ] [007] `rule init/list/validate/show/for-agent` 均按定义工作，且不修改已有散装规则文件。
- [ ] [008] Skill 可按路径、name 和对象方式解析，子 Agent 追加去重，不支持的 disable/override 写法被拒绝。
- [ ] [009] 当前仅有 `SKILL.md` front matter 的 AZ-SKILLS Skill 可直接引用；双元数据不一致时校验失败。
- [ ] [010] `raw-prompt` 产物包含生成头、最终 prompt、按 severity 分组的 Rule 正文和 Skill 列表。
- [ ] [011] Codex 与 OpenCode renderer 各自通过契约 fixture/validator，输出可由验收时目标工具版本加载。
- [ ] [012] Hermes 和 Claude Code 在 renderer 未实现时返回明确不支持错误，不生成伪产物。
- [ ] [013] 相同输入重复构建时，忽略生成时间后的正文和内容摘要一致。
- [ ] [014] 任一源或引用校验失败时不产生半成品，已有 dist 产物保持不变。
- [ ] [015] 漂移检查准确区分 clean、drift、missing、unmanaged、invalid，且检查过程不写文件。
- [ ] [016] 漂移检查按约定返回 0、2、3、4 退出码，可由外部计划任务稳定调用。
- [ ] [017] `sync --dry-run` 不写文件，并列出将创建、更新、跳过、冲突和可选删除的项目。
- [ ] [018] 普通 `sync` 只创建或更新当前 Agent/target 的受管文件，遇到 unmanaged 同名文件时停止。
- [ ] [019] 未指定 `--prune` 时同步不删除文件；指定后也只删除受管清单中的过期文件。
- [ ] [020] 同步中途失败时目标工作区保持原状或可完整回滚，返回非零退出码。
- [ ] [021] `draft import` 只写入 `drafts/`，不创建或覆盖 `agents/`，无法映射字段被记录。
- [ ] [022] 所有路径引用均限制在项目根目录内，路径逃逸和链接逃逸被拒绝。
- [ ] [023] Agent Hub 不修改 `manifest.yaml`、现有 Skill 源文件和非结构化 Rule 文件。
- [ ] [024] 批量校验和构建输出成功、警告、失败汇总，并提供机器可读模式。

## 测试用例

### MT001 初始化幂等性

- **测试步骤**：在空临时目录执行两次 `az-agent-hub init`，第二次执行前修改示例 Agent。
- **预期结果**：首次创建完整结构；第二次不覆盖修改后的文件，并报告跳过项。
- **验收标准**：[001]

### MT002 Agent 字段校验

- **测试步骤**：分别构造缺少必填字段、非法 name、非法 SemVer、temperature 超范围和未知字段的 Agent。
- **预期结果**：每个错误均包含文件、字段路径、原因和修复建议，退出码非零。
- **验收标准**：[002]

### MT003 继承展开

- **测试步骤**：建立三层 Agent 链，父子分别定义 prompt、rules、skills、tags 和模型参数，执行 `agent show --resolved`。
- **预期结果**：覆盖、追加、去重和来源追踪符合规范。
- **验收标准**：[003]

### MT004 继承异常

- **测试步骤**：分别构造缺失父文件、A-B-A 循环和四条继承边。
- **预期结果**：三种情况均阻止构建，并输出完整问题链路。
- **验收标准**：[004]

### MT005 Rule 解析与继承控制

- **测试步骤**：父 Agent 引用三个 severity 的 Rule，子 Agent 禁用一个、覆盖一个并新增一个。
- **预期结果**：最终 Rule 内容、顺序、severity 和来源正确；不存在 id 被拒绝。
- **验收标准**：[005]、[006]

### MT006 Rule 命令集

- **测试步骤**：依次执行 `rule init`、`list`、`validate`、`show`、`for-agent`，并在 `rules/` 放置一个既有散装 Markdown 文件。
- **预期结果**：结构化 Rule 正常管理；散装文件不被修改或误报为结构化 Rule。
- **验收标准**：[007]

### MT007 Skill 兼容与继承

- **测试步骤**：引用一个仅有合法 `SKILL.md` front matter 的现有 Skill，再测试双元数据冲突和子 Agent 重复追加。
- **预期结果**：现有 Skill 可解析；冲突被拒绝；重复项只保留一份。
- **验收标准**：[008]、[009]

### MT008 Raw Prompt 构建

- **测试步骤**：构建含 prompt、三类 Rule 和多个 Skill 的 Agent 到 `raw-prompt`。
- **预期结果**：生成头、正文分组、Skill 列表和摘要完整，顺序稳定。
- **验收标准**：[010]、[013]

### MT009 Codex 与 OpenCode 构建

- **测试步骤**：使用契约 fixture 分别构建 Codex、OpenCode target，并运行各自 validator/加载验证。
- **预期结果**：两个 target 均符合验收时的目标工具契约；通用 resolver 输出一致。
- **验收标准**：[011]

### MT010 未实现 Target

- **测试步骤**：请求构建 Hermes 和 Claude Code。
- **预期结果**：输出明确的不支持诊断、非零退出码，`dist/` 无伪产物。
- **验收标准**：[012]

### MT011 构建原子性

- **测试步骤**：先生成有效产物，再破坏一个 Rule 引用并重新批量构建。
- **预期结果**：构建失败，先前有效产物内容不变，不存在临时半成品。
- **验收标准**：[014]

### MT012 漂移状态与退出码

- **测试步骤**：依次制造 clean、内容变更、文件缺失、unmanaged 冲突和源无效五种状态并执行漂移检查。
- **预期结果**：状态和退出码分别符合规范，检查前后文件系统内容摘要不变。
- **验收标准**：[015]、[016]

### MT013 同步安全

- **测试步骤**：执行 dry-run、普通 sync、制造 unmanaged 同名文件、测试无/有 `--prune`，并模拟写入失败。
- **预期结果**：计划准确；只更新受管文件；冲突停止；删除受参数保护；失败不破坏原目标。
- **验收标准**：[017]、[018]、[019]、[020]

### MT014 反向草稿

- **测试步骤**：从一个含 target 专属字段的文件执行 `draft import`。
- **预期结果**：仅生成带来源和未审核标记的 draft，未映射字段有诊断，`agents/` 不变。
- **验收标准**：[021]

### MT015 路径与既有仓库保护

- **测试步骤**：构造 `..`、绝对路径和指向根目录外的链接引用；执行 validate、build、drift 和 sync 前后比较 `manifest.yaml`、现有 Skill、散装 Rule。
- **预期结果**：逃逸路径被拒绝；受保护文件内容不变。
- **验收标准**：[022]、[023]

### MT016 批量与机器可读输出

- **测试步骤**：批量处理一组成功、警告和失败 Agent，并启用机器可读输出。
- **预期结果**：人类输出有汇总，机器输出结构稳定且包含逐项状态。
- **验收标准**：[024]

## 需求追踪与待定项

| 项目 | 当前决策 |
|------|----------|
| 功能拆分 | 作为一个复杂统一功能管理，内部按 resolver/renderer/drift/sync 分模块 |
| 规范模式 | strict |
| v1.0 target | raw-prompt、Codex、OpenCode |
| 独立技术规范 | 推荐，需后续执行 `/az-sdd-arch 0001` |
| Codex 文件契约 | `dist/codex/<name>.toml`，同步到 `.codex/agents/<name>.toml`；必填 name、description、developer_instructions，并以官方样例固化 fixture |
| 定时检查 | 提供只读命令和稳定退出码；外部调度安装不在 v1.0 范围 |
| 原始需求结尾 | 来源文件在 raw-prompt 生成头描述处结束；未提供的 target 细节按本规范的适配器约束闭合 |

## 审核记录

| 日期 | 审核人 | 结果 | 说明 |
|------|--------|------|------|
| - | - | 待审核 | 新建规范默认状态为草稿 |
