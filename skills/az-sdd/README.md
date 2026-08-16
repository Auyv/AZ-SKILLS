# AZ-SDD v4.1 — 单一自适应流程的规范驱动开发技能族

AZ-SDD 是一组面向 AI 编程助手的规范驱动开发（Spec-Driven Development）技能。它把需求、技术决策、实现、自动化测试、人工验收、反馈和项目状态组织成可追踪的项目档案。

v4 在 v3.2 可执行规则基础上，引入 `baseline + changes + archive`：当前已生效行为与正在开发的增量变更分离，一次 change 聚合规范、设计、任务、证据、验收和反馈。

同步到 Agent 时，九个用户可调用技能会被平铺安装；公共脚本、契约和参考资料会安装到不可调用的 `az-sdd-shared/` 运行时目录，供这些技能通过相对路径共享。

## 核心原则

- **显式进入**：只有用户明确调用 AZ-SDD/SDD 流程时才启用，不劫持普通开发请求。
- **先对齐再实现**：所有 change 都保留最小规范和验证；简单任务自动精简，风险只增加必要门禁。
- **Agent 判断，脚本校验**：Agent 负责理解和取舍，脚本负责状态、索引和一致性检查。
- **证据驱动完成**：没有自动化测试、人工验收或明确关闭理由，不把工作标记为完成。
- **当前态与历史分离**：`STATUS.md` 是当前状态入口，`TRACK.md` 只记录关键事件。
- **基线与增量分离**：baseline 只保存已生效行为；开发期间只修改 change，验收归档后再合并。
- **冲突可检测、写入可回滚**：delta 记录基线哈希，archive 前执行门禁，写入失败自动恢复备份。

## 推荐入口

优先显式调用：

```text
/az-sdd-next
```

它会读取当前项目状态、未闭环反馈和前置条件，然后推荐下一步。也可以直接调用某个阶段：

```text
/az-sdd-init 初始化 AZ-SDD 项目档案
/az-sdd-spec 为登录功能创建 CHG-0001
/az-sdd-arch 为 CHG-0001 创建设计
/az-sdd-coder 按已检查 change 实现 CHG-0001
/az-sdd-tester 验收 CHG-0001
/az-sdd-track archive CHG-0001
/az-sdd-feedback 将这条反馈纳入闭环
```

普通地说“写代码”“跑测试”“修 bug”“看进度”时，继续使用项目原有流程。

## 单一自适应流程

用户不选择 lite、standard 或 strict。AZ-SDD 根据任务客观风险自动决定是否增加 design、完整测试、人工验收、回滚计划或范围澄清。

| 路径 | 触发条件 | 流程 |
|---|---|---|
| 快速路径 | 单模块、可回滚、影响明确、现有测试充分 | change → implement/evidence → acceptance → archive |
| 风险展开 | 多模块、API、数据、安全、硬件、部署、不可逆、测试不足或范围未知 | change → 条件门禁 → implement → acceptance → archive |

快速路径允许 Agent 连续完成，但不会省略 proposal、delta、tasks、evidence、acceptance 和 baseline 冲突检查。

## 九个技能

| Skill | 作用 |
|---|---|
| `az-sdd-next` | 主入口：读取状态、检查一致性并推荐下一步 |
| `az-sdd-init` | 初始化 `az-sdd/` 项目档案 |
| `az-sdd-spec` | 澄清需求、生成规范和验收标准 |
| `az-sdd-arch` | 记录复杂技术决策、运行与验证条件 |
| `az-sdd-coder` | 消费已通过检查的规范和任务，实施并保存证据 |
| `az-sdd-tester` | 组织人工测试、异常处理和用户验收 |
| `az-sdd-track` | 查看状态、重建摘要并生成项目汇报 |
| `az-sdd-feedback` | 编排反馈的澄清、计划、修复、验证与关闭 |
| `az-sdd-rules` | 解释规则并执行状态和跨制品一致性检查 |

## 主流程

```text
init / migrate
  ↓
change-init → proposal + spec.delta
  ↓
design（风险触发时）→ analyze
  ↓
tasks → coder → evidence
  ↓
tester → acceptance
  ↓
change-validate → archive → merge baseline
```

反馈可从任一阶段进入 `feedback`，验证通过后回到主流程或关闭。

## 项目档案结构

v4 默认结构：

```text
az-sdd/
├── PROJECT.md
├── STATUS.md
├── TRACK.md
├── .az-sdd-v4.json
├── baseline/specs/
├── changes/CHG-0001/
│   ├── proposal.md
│   ├── spec.delta.md
│   ├── design.md
│   ├── tasks.md
│   ├── evidence.md
│   ├── acceptance.md
│   └── feedback.md
├── archive/
└── feedback/
```

迁移后的项目同时保留 v3 `spec/`、`arch/`、`coding/`、`test/` 等目录；读取器优先使用 v4 baseline，原档案可继续回看。

## v4 CLI

```powershell
python az_sdd.py init --project C:\project --title "项目" --dry-run
python az_sdd.py migrate --from v3 --to v4 --project C:\project --dry-run
python az_sdd.py change-init CHG-0001 --project C:\project --title "变更" --target specs/0001-name.spec.md --operation MODIFIED --risk-factor public_api --dry-run
python az_sdd.py change-validate CHG-0001 --project C:\project
python az_sdd.py archive CHG-0001 --project C:\project --dry-run
```

所有新建、迁移和归档命令默认不落盘；确认结果后显式改用 `--apply`。

## 安装与同步

本目录是 AZ-SKILLS 仓库中的技能源码包，`manifest.yaml` 是同步目标清单。

```powershell
# 查看状态
python sync_skills.py --list

# 仅预览，不复制
python sync_skills.py --dry-run

# 同步 AZ-SDD 技能族到已启用平台
python sync_skills.py --skill az-sdd

# 同步到指定平台
python sync_skills.py --to codex
python sync_skills.py --to claude_code
python sync_skills.py --to opencode
```

同步到 Agent 后，通常需要新开会话才能刷新技能列表。

## 文档

- [使用指南](./docs/usage.md)
- [v4 开发流程](./docs/workflow.md)
- [无 Agent 自动编排时的手动指南](./docs/manual.md)
- [v3 → v4 兼容与迁移说明](./docs/migration-v3.md)
- [v3.2 优化基线审计](./docs/baseline-audit.md)

## 适用范围

适合个人开发者、小团队、教学项目、存量工程，以及需要人工验收、设备验证或反馈闭环的软硬件项目。

不适合以下情况：

- 用户只想快速试验且不希望产生任何档案。
- 项目状态已经由 Jira、Linear、CI 和 QA 平台严格自动管理，且不需要本地 Markdown 档案。
- 项目有高等级合规要求，但尚未接入权限、签名和审计系统。
