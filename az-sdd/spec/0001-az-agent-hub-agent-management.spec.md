# [0001] az-agent-hub Agents 组织与管理

## 基本信息

| 字段 | 内容 |
|------|------|
| 功能编号 | 0001 |
| 功能名称 | az-agent-hub Agents 组织与管理 |
| 所属模块 | az-agent-hub |
| 规范模式 | strict |
| 状态 | 已批准 |
| 创建日期 | 2026-07-15 |
| 最后更新 | 2026-07-17 |
| 需求来源 | `C:\Users\jpz\Desktop\az-agent-hub.txt` |

## 背景与定位

AZ-SKILLS 当前以 `skills/` 作为 Skill 的单一真相来源，并通过 `manifest.yaml` 和 `sync_skills.py` 分发到多个 AI 编程工具。仓库尚缺少同等级的 Agent 组织能力：Agent prompt、规则组合、技能组合和目标工具格式之间没有统一声明、编译、漂移检查与同步入口。

本功能新增命令行工具 `az-agent-hub`。用户只维护 `agents/<name>.agent.yaml`，工具负责校验、继承展开、规则与技能解析、跨工具编译、漂移检查和显式同步。

## 核心原则

1. `agents/<name>.agent.yaml` 是 Agent 定义的唯一权威源。
2. 读取、校验和漂移检查不得修改任何文件。
3. `init`、`build`、`sync`、`rule init` 和 `draft import` 等写操作必须由用户显式执行。
4. `sync` 不得覆盖无法确认由 az-agent-hub 管理的文件。
5. `rules/` 和 `skills/` 保持当前文件结构与单一真相源，不为 Agent Hub 创建旁路元数据。
6. Rule 和 Skill 默认以运行时引用交给 Agent；新建或重新加载 Agent 时读取最新文件，源文件更新不要求重新生成 Agent。
7. target 差异封装在 renderer 中，不得污染 Agent 的通用声明格式。
8. 与当前 `skills/`、`rules/` 和 `manifest.yaml` 共存，不破坏既有 Skill 同步流程。

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
- 当前 `rules/*.md` 的实时引用、继承、禁用和替换。
- 当前 `skills/**/SKILL.md` 的递归发现、实时引用和继承追加。
- `raw-prompt`、`codex`、`opencode` 三个 target renderer。
- 可扩展 renderer 接口，为 `hermes`、`claude-code` 预留 target 名称和目录。
- 确定性编译、漂移检查、显式同步和反向草稿。
- 可供外部计划任务调用的稳定退出码；v1.0 不自行安装或管理定时任务。

### v1.0 不包含

- 图形界面或 Web 服务。
- Agent 多继承。
- Skill 的 disable/override 机制。
- 为 Rule 创建 `rule.yaml`，或为 Skill 创建 `skill.yaml` 等重复元数据。
- 自动复制、改写或迁移当前 `rules/`、`skills/` 文件结构。
- 自动修改 `agents/`、现有 `skills/`、现有规则文件或目标工作区。
- 自动下载模型、Skill、Rule 或 target 插件。
- Hermes、Claude Code renderer 的正式交付；其实现可在后续规范中补充。
- 云端账户、多人权限和远程数据库。

## 项目目录

显式执行初始化后，在项目根目录创建：

```text
agents/                       # Agent 声明，唯一真相源
├── base.agent.yaml
└── mcu-dev.agent.yaml
rules/                        # Rule 短文件名的默认查找目录
skills/                       # 既有 Skill 单一真相源，递归发现 SKILL.md
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
| `rules` | 否 | list | Rule 短文件名、相对 `rules/` 的路径或用户提供的绝对路径 |
| `skills` | 否 | list | Skill 目录、`SKILL.md` 路径或唯一 Skill name |
| `model` | 否 | string | target 支持时输出；不支持时给出明确诊断 |
| `temperature` | 否 | number | 闭区间 `[0, 2]` |
| `max_tokens` | 否 | integer | 大于 0 |
| `changelog` | 否 | list | 每项至少包含 `version` 和非空描述 |
| `disable_rules` | 否 | list | 仅引用父链已存在的 Rule 路径 |
| `override_rules` | 否 | map | key 为父 Rule 路径，value 为替换后的 Rule 文件路径 |
| `tags` | 否 | list | 字符串列表，合并后去重 |

字段名统一使用 `disable_rules` 和 `override_rules`；不接受无下划线别名，避免出现两套格式。

### 继承规则

- 只允许单继承。
- 从当前 Agent 到最顶层父 Agent 的边数不得超过 3。
- 禁止直接或间接循环引用。
- `prompt`、`model`、`temperature`、`max_tokens`、`version`、`changelog`：子值覆盖父值。
- `rules`、`skills`：父项在前、子项在后，分别按规范化文件路径和 `SKILL.md` 路径去重。
- `tags`：父子合并去重，并保持首次出现顺序。
- `disable_rules`：在完整父链展开后按规范化路径移除对应 Rule 引用。
- `override_rules`：在完整父链展开后用新 Rule 文件引用替换父 Rule 引用，保留列表位置并更新来源标记。
- `name` 和 `description` 不继承；它们描述当前 Agent。
- 每个展开字段和 Rule/Skill 条目均保留来源 Agent，供 `show --resolved` 和诊断输出使用。

### 校验

- 必填字段、类型、name 正则、文件名一致性和 SemVer 均须校验。
- `extends`、相对 Rule 路径和 Skill 入口必须存在，解析后的路径不得通过 `..` 或链接逃逸各自允许范围。
- Rule 短文件名和相对路径默认基于 `<项目根>/rules/` 解析；用户显式填写的绝对 Rule 路径不受项目根目录限制。
- 绝对 Rule 路径由用户自行保证有效性和目标环境可访问性；Agent Hub 不复制、不改写、不提供跨机器可移植保证，但在校验或加载失败时必须报告原路径和原因。
- Rule 必须是非空、可读取的 UTF-8 Markdown 文件；Skill 必须解析到唯一且非空的 `SKILL.md`。
- `disable_rules`、`override_rules` 必须引用父链中存在的规范化 Rule 路径。
- YAML 未知字段默认报错；只有配置显式允许兼容模式时才降级为警告。
- 一次校验尽量汇总全部独立错误，输出文件、字段路径、原因和修复建议。

## Rules 系统

### 当前结构即真相源

Rule 是 Agent 执行任务时必须遵守的底线，直接使用仓库现有 Markdown 文件，不引入 `rule.yaml`、Rule 目录脚手架或复制文件。例如：

```text
rules/
├── AZ_GLOBAL.rules.md
└── c_code.rules.md
```

Rule 列表优先使用简洁的文件名。没有根目录或盘符的值默认相对 `<项目根>/rules/` 解析；也允许用户直接填写操作系统绝对路径：

```yaml
rules:
  - AZ_GLOBAL.rules.md
  - c_code.rules.md
  - 'C:/shared/agent-rules/company.rules.md'
```

- `AZ_GLOBAL.rules.md` 解析为 `<项目根>/rules/AZ_GLOBAL.rules.md`。
- `subdir/custom.rules.md` 解析为 `<项目根>/rules/subdir/custom.rules.md`。
- Windows 盘符路径、UNC 路径和其他操作系统识别的绝对路径按原路径使用；YAML 中建议使用单引号或正斜杠，避免反斜杠转义。
- 展开、去重、`disable_rules` 和 `override_rules` 均以解析后的规范化绝对路径作为身份。

### 加载与优先级

- 默认采用实时引用模式：生成的 Agent 只保存 Rule 路径和“执行任务前完整读取并遵循”的强制指令，不嵌入 Rule 正文。
- 每次新建或重新加载 Agent 时，必须按继承展开后的顺序重新读取全部 Rule 文件；Rule 修改后无需重新构建 Agent，下次加载即生效。
- 已经运行中的 Agent 不要求热更新；需要应用新 Rule 时重新加载 Agent。
- Rule 的约束优先于 Agent prompt 和 Skill 指令；发生冲突时以 Rule 为准并报告冲突。
- `disable_rules`、`override_rules` 只改变当前 Agent 的引用清单，不修改 Rule 源文件。
- 需要离开源仓库使用时，用户可显式选择快照嵌入模式；工具不得静默把实时引用降级为内容副本。

### Rule 校验与查看

| 命令 | 行为 |
|------|------|
| `rule list` | 列出 `rules/` 下可引用的 Markdown 文件及相对路径 |
| `rule validate <path>` | 按默认 `rules/` 或绝对路径规则解析，并只读校验文件存在、非空、UTF-8 |
| `rule show <path>` | 显示指定 Rule 的当前完整内容 |
| `rule for-agent <agent>` | 显示 Agent 展开后的 Rule 路径、加载顺序和来源 Agent |

## Skills 系统

### 当前结构即真相源

Skill 直接复用当前仓库的目录，不新增 `skill.yaml`。每个实际 Skill 以 `SKILL.md` 为入口，并可继续拥有 `scripts/`、`references/`、`docs/`、`evals/`、`MANUAL.md` 等配套文件：

```text
skills/<skill-path>/
├── SKILL.md
├── scripts/        # 可选
├── references/     # 可选
├── docs/           # 可选
└── evals/          # 可选
```

- 递归发现全部 `skills/**/SKILL.md`，因此支持顶层 Skill、`az-sdd.v3/<skill>/` 和 `hq_tester_skills/<skill>/` 等嵌套结构。
- 没有 `SKILL.md` 的技能族容器不是可直接引用的 Skill。
- Skill 的 name、version、description 直接读取 `SKILL.md` YAML front matter；不得复制到第二份元数据文件。
- Skill 目录内的配套文件保持原位，由 `SKILL.md` 中的相对路径继续引用。

支持目录路径、入口路径或唯一 name：

```yaml
skills:
  - skills/az-skill-creator
  - skills/az-sdd.v3/az-sdd-spec/SKILL.md
  - ctp-mpu-debug-build
```

### 加载与优先级

- 默认采用实时引用模式：target 原生支持 Skill 引用时传递 `SKILL.md` 路径；否则在 Agent 指令中注册路径和触发时读取要求。
- 新建或重新加载 Agent 时重新发现并注册引用的 Skill；Skill 内容修改后无需重新构建 Agent，下次加载或下次触发该 Skill 时读取最新内容。
- Skill 按任务触发条件使用，不要求每次任务预先完整读取所有 Skill；一旦触发，必须完整读取对应 `SKILL.md` 及其要求的资源。
- Skill 指令不得覆盖 Rule；冲突时停止相关 Skill 动作并报告冲突。
- `skills` 继承只追加并按规范化 `SKILL.md` 路径去重，v1.0 不提供 disable/override。
- 按 name 查找出现多个匹配时必须报歧义，要求用户改用路径引用。
- `skill list/validate/show` 必须递归处理当前结构，且全程只读。

## Target 与编译

### v1.0 target

| target | 状态 | 输出要求 |
|--------|------|----------|
| `raw-prompt` | 必须实现 | 纯文本/Markdown，包含 prompt、Rule/Skill 实时引用路径、加载要求和生成标记；显式快照模式才嵌入正文 |
| `codex` | 必须实现 | 生成 Codex custom agent TOML，并通过仓库内 fixture/validator 验证 |
| `opencode` | 必须实现 | 由独立 renderer 按实现时 OpenCode 可加载契约生成，并通过仓库内 fixture/validator 验证 |
| `hermes` | 预留 | v1.0 返回明确的“不支持”诊断，不生成伪产物 |
| `claude-code` | 预留 | v1.0 返回明确的“不支持”诊断，不生成伪产物 |

### Codex renderer 契约

- dist 产物为 `dist/codex/<name>.toml`，显式同步的项目级目标为 `.codex/agents/<name>.toml`；个人级目标路径由配置显式选择。
- TOML 必填字段映射：Agent `name` -> `name`，Agent `description` -> `description`，展开后的 prompt、Rule 必须加载指令和 Skill 使用边界 -> `developer_instructions`。
- 构建 Codex target 时，源 Agent 的 `description` 必须非空；缺失时 target 校验失败，不自动编造说明。
- Agent `model` 映射为 Codex `model`；其他字段仅在当前 Codex 配置契约支持时映射。
- Rules 在 `developer_instructions` 中保留仓库路径，并要求每次 Agent 开始执行前完整读取；不得默认内嵌旧快照。
- Skills 优先映射到 Codex 支持的 `skills.config` 并保留当前 `SKILL.md` 路径；若无法无损映射，则在 `developer_instructions` 中注册路径和触发时读取要求，并给出显式警告。
- `temperature`、`max_tokens` 或其他 Codex 不支持的字段不得静默写入；renderer 按配置选择“警告并忽略”或“严格失败”，strict 模式默认失败。
- 实现时必须把当时官方 custom agent 样例和必填字段固化为 fixture，并记录验证所用 Codex 版本，防止后续格式演进造成无提示漂移。

### 生成产物约束

- 默认写入 `dist/<target>/`，不得直接写目标工具工作区。
- 每个产物包含生成标记、源 Agent 相对路径、源版本、引用路径和 Agent 声明内容摘要。
- `raw-prompt` 顶部包含 `<!-- Generated by az-agent-hub from <source> at <time> -->`。
- 实时引用模式的摘要包含 Agent 声明和规范化引用路径，不包含 Rule/Skill 当前正文；Rule/Skill 内容更新不应制造必须重建 Agent 的假漂移。
- 显式快照模式的摘要包含嵌入内容；Rule/Skill 更新后应报告快照已过期。
- 同一输入在忽略生成时间元数据后必须得到相同正文和相同内容摘要。
- 漂移比较使用对应模式的规范化摘要，不得因时间戳变化产生假漂移。
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

实时引用模式下，Rule/Skill 正文变化不属于 Agent 产物 drift；检查必须验证引用仍存在且可读。引用被删除、移动、变成歧义或逃逸允许范围时属于 `invalid`。快照模式下，引用内容变化属于 `drift`。

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
az-agent-hub rule list|validate|show|for-agent ...
az-agent-hub skill list|validate|show ...
az-agent-hub draft import --target <target> --source <path>
```

所有命令支持 `--root` 或从当前目录向上查找 `az-agent-hub.config.yaml`。错误输出必须包含失败对象、原因和修复建议；批量命令输出成功、警告、失败汇总。

## 配置与当前项目集成

`az-agent-hub.config.yaml` 至少包含：

- Agent、Rule 文件、Skill 递归发现、dist、drafts 根目录。
- 启用的 target 及 renderer 配置。
- target 工作区路径。
- 是否从当前 `manifest.yaml` 的 `agent_targets` 只读继承 target 名称和基础路径。
- 未知字段策略和路径解析策略。

集成约束：

1. `manifest.yaml` 仍由现有 Skill 同步流程维护，Agent Hub 默认只读。
2. `skills/**/SKILL.md` 及其所在目录仍是 Skill 单一真相来源，Agent Hub 不创建 `skill.yaml` 或 Agent 专用副本。
3. `<项目根>/rules/` 是 Rule 短文件名的默认查找目录；显式绝对路径是用户管理的外部 Rule 单一真相源。Agent Hub 不创建 `rule.yaml`、包装目录或副本。
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

- schema/validator：Agent、Rule 文件引用、Skill 入口引用和配置校验。
- resolver：继承、路径引用、disable/override、递归 Skill 发现和来源追踪。
- renderers：每个 target 一个独立适配器。
- drift：规范化摘要、状态判定和退出码。
- sync：受管文件清单、dry-run、原子写入和 prune。
- drafts：target 反向映射为草稿。
- cli：参数解析和结果展示。

### 安全与可维护性

- `extends`、Skill 和相对 Rule 引用在解析后校验边界，禁止 `..` 或链接逃逸允许范围；用户显式提供的绝对 Rule 路径是唯一例外。
- Agent Hub 在校验、构建和漂移检查阶段不执行 Rule、Skill 或其脚本；Agent 运行时仅在 Skill 触发且 Rule 允许时按 Skill 流程执行。
- 生成标记不能作为唯一信任依据；同时校验受管清单和目标相对路径。
- 诊断中避免输出环境变量、凭据和目标文件中的敏感内容。
- renderer 采用注册表/协议扩展，新增 target 不修改 resolver 核心逻辑。

## 输入项

| 输入 | 来源 | 必填 |
|------|------|------|
| Agent YAML | `agents/*.agent.yaml` | 是 |
| Rule 文件 | 默认 `<项目根>/rules/` 下文件，也可为用户提供的绝对路径 | Agent 引用时必填 |
| Skill 入口与资源 | 当前 `skills/**/SKILL.md` 及其所在目录资源 | Agent 引用时必填 |
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
- YAML 语法错误、编码错误、未知字段、重复引用分别定位到具体文件和字段。
- 父文件缺失、循环继承、深度超限、相对路径逃逸必须阻止构建；显式绝对 Rule 路径不按逃逸处理。
- Rule 文件缺失、Skill name 重名、Skill 路径未指向唯一 `SKILL.md` 时必须阻止歧义解析。
- 绝对 Rule 路径不存在、不可读或在目标环境不可访问时，必须显示原路径和失败原因；工具不负责修正路径。
- target 未支持、target 工作区不存在、权限不足时不得留下部分写入。
- unmanaged 文件冲突必须停止同步。
- 生成时间变化不得单独造成 drift。
- Windows 路径大小写、分隔符和 `~` 展开后使用规范化路径比较。

## 验收标准

- [ ] [001] 显式执行 `init` 后创建要求的目录、配置和示例，重复执行不覆盖已有文件。
- [ ] [002] 缺少 `name`、`version` 或 `prompt`，或 name/SemVer/数值范围不合法时，校验失败并定位字段。
- [ ] [003] 单继承按规定覆盖和追加字段，展开结果保留每项来源。
- [ ] [004] 循环继承、父文件缺失或继承深度超过 3 时，构建被阻止并给出继承链。
- [ ] [005] `disable_rules` 和 `override_rules` 按规范化 Rule 路径正确作用于父链，且不修改 Rule 源文件。
- [ ] [006] Agent 每次新建或重新加载时完整读取当前 Rule 文件；Rule 修改后不重建 Agent 也能在下次加载生效，并优先于 prompt/Skill。
- [ ] [007] `rule list/validate/show/for-agent` 可直接处理当前 `rules/*.md`，全程不创建、复制或修改 Rule 文件。
- [ ] [008] Skill 可按目录路径、`SKILL.md` 路径和唯一 name 解析，支持嵌套技能族并按入口路径去重。
- [ ] [009] Skill 修改后不重建 Agent 也能在下次加载或触发时读取最新内容；Skill 配套目录保持可访问，且 Skill 不得覆盖 Rule。
- [ ] [010] `raw-prompt` 默认产物包含生成头、最终 prompt、Rule/Skill 引用路径和加载要求，不内嵌旧快照；显式快照模式可生成自包含产物。
- [ ] [011] Codex 与 OpenCode renderer 各自通过契约 fixture/validator，输出可由验收时目标工具版本加载。
- [ ] [012] Hermes 和 Claude Code 在 renderer 未实现时返回明确不支持错误，不生成伪产物。
- [ ] [013] 相同 Agent 声明和引用路径重复构建时摘要一致；实时引用内容变化不产生 Agent drift，快照模式内容变化产生 drift。
- [ ] [014] 任一源或引用校验失败时不产生半成品，已有 dist 产物保持不变。
- [ ] [015] 漂移检查准确区分 clean、drift、missing、unmanaged、invalid，且检查过程不写文件。
- [ ] [016] 漂移检查按约定返回 0、2、3、4 退出码，可由外部计划任务稳定调用。
- [ ] [017] `sync --dry-run` 不写文件，并列出将创建、更新、跳过、冲突和可选删除的项目。
- [ ] [018] 普通 `sync` 只创建或更新当前 Agent/target 的受管文件，遇到 unmanaged 同名文件时停止。
- [ ] [019] 未指定 `--prune` 时同步不删除文件；指定后也只删除受管清单中的过期文件。
- [ ] [020] 同步中途失败时目标工作区保持原状或可完整回滚，返回非零退出码。
- [ ] [021] `draft import` 只写入 `drafts/`，不创建或覆盖 `agents/`，无法映射字段被记录。
- [ ] [022] Rule 短文件名默认从 `<项目根>/rules/` 加载；显式绝对 Rule 路径可位于项目外且原样保留；其他相对引用的 `..`/链接逃逸被拒绝，无效绝对路径给出明确错误。
- [ ] [023] Agent Hub 不修改 `manifest.yaml`、任何现有 Skill/配套资源和 Rule 文件，也不创建 `skill.yaml`、`rule.yaml` 或内容副本。
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

### MT005 Rule 路径继承控制

- **测试步骤**：父 Agent 引用 `AZ_GLOBAL.rules.md`，子 Agent 追加、禁用和替换 Rule 路径；随后修改被引用 Rule 并重新加载 Agent。
- **预期结果**：引用顺序和来源正确，源文件未被修改；重新加载后读取到新内容，且 Rule 冲突时优先于 prompt/Skill。
- **验收标准**：[005]、[006]

### MT006 Rule 命令集

- **测试步骤**：针对当前 `AZ_GLOBAL.rules.md`、`c_code.rules.md` 执行 `list`、`validate`、`show`、`for-agent`。
- **预期结果**：命令直接处理现有 Markdown 文件，输出当前内容和加载顺序，文件系统没有新增或修改。
- **验收标准**：[007]

### MT007 Skill 当前结构与实时加载

- **测试步骤**：分别按路径和 name 引用顶层 Skill、`az-sdd.v3/az-sdd-spec` 和带 `scripts/` 的 Skill；修改其中一个 `SKILL.md` 后重新加载 Agent。
- **预期结果**：递归发现与去重正确，配套资源仍可访问；重新加载或触发时读取新内容，没有生成 `skill.yaml` 或副本。
- **验收标准**：[008]、[009]

### MT008 Raw Prompt 构建

- **测试步骤**：构建含 prompt、Rule 文件和多个 Skill 引用的 Agent 到 `raw-prompt`，再分别测试默认模式和显式快照模式。
- **预期结果**：默认产物保存路径与加载要求而非内容副本；快照模式生成自包含内容；两种模式摘要规则符合规范。
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

- **测试步骤**：先生成有效产物，再删除一个 Rule 文件或让 Skill name 产生歧义并重新批量构建。
- **预期结果**：构建失败，先前有效产物内容不变，不存在临时半成品。
- **验收标准**：[014]

### MT012 漂移状态与退出码

- **测试步骤**：依次制造 clean、实时引用内容变更、快照内容变更、引用缺失、unmanaged 冲突和源无效状态并执行漂移检查。
- **预期结果**：实时引用内容变更仍为 clean 且重新加载可见；快照内容变更为 drift；其余状态和退出码符合规范，检查过程只读。
- **验收标准**：[015]、[016]

### MT013 同步安全

- **测试步骤**：执行 dry-run、普通 sync、制造 unmanaged 同名文件、测试无/有 `--prune`，并模拟写入失败。
- **预期结果**：计划准确；只更新受管文件；冲突停止；删除受参数保护；失败不破坏原目标。
- **验收标准**：[017]、[018]、[019]、[020]

### MT014 反向草稿

- **测试步骤**：从一个含 target 专属字段的文件执行 `draft import`。
- **预期结果**：仅生成带来源和未审核标记的 draft，未映射字段有诊断，`agents/` 不变。
- **验收标准**：[021]

### MT015 Rule 默认目录、绝对路径与仓库保护

- **测试步骤**：使用 `AZ_GLOBAL.rules.md`、`subdir/custom.rules.md`、有效/无效绝对 Rule 路径和 `..`/链接逃逸相对路径；执行 validate、build、drift 和 sync 前后比较 `manifest.yaml`、全部 Skill/配套资源和 Rule，并检查是否出现副本或旁路元数据。
- **预期结果**：短文件名和普通相对路径从默认 `rules/` 正确解析；有效绝对路径原样使用；无效绝对路径显示原路径与原因；相对逃逸被拒绝；任何 Rule/Skill 均未被复制或修改。
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
| Rules 组织 | 短文件名默认从 `<项目根>/rules/` 解析，也支持用户负责有效性的绝对路径；每次 Agent 加载前完整读取，Rule 是执行底线 |
| Skills 组织 | 递归引用当前 `skills/**/SKILL.md` 及其目录资源，重新加载或触发时读取最新内容 |
| 引用模式 | 默认实时引用；离开源仓库时由用户显式选择快照嵌入，不允许静默降级 |
| 定时检查 | 提供只读命令和稳定退出码；外部调度安装不在 v1.0 范围 |
| 原始需求结尾 | 来源文件在 raw-prompt 生成头描述处结束；未提供的 target 细节按本规范的适配器约束闭合 |

## 审核记录

| 日期 | 审核人 | 结果 | 说明 |
|------|--------|------|------|
| 2026-07-17 | 用户 | 提交审核 | 规格由草稿进入审核中 |
| 2026-07-17 | 用户 | 批准 | 用户明确批准规格并要求开始实现；实现前需先完成 strict 模式技术规范 |
