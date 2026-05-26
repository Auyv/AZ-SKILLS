# [0001] Skill 反馈收集与每日进化

## 基本信息

| 字段 | 内容 |
|------|------|
| 功能编号 | 0001 |
| 功能名称 | Skill 反馈收集与每日进化 |
| 所属模块 | skill-feedback |
| 状态 | 开发中 |
| 创建日期 | 2026-05-26 |
| 最后更新 | 2026-05-26 |

## 功能描述

提供一个本地反馈单工具，让用户通过桌面脚本提交对某个 skill 的使用痛点；每日 1 点自动整理未处理反馈，基于理性判断生成进化建议，必要时备份并修改对应 skill，最后生成本次进化汇报。

## 使用场景

用户在使用某个 skill 后发现痛点或改进建议，希望快速记录下来，不打断当前工作。系统在固定时间集中处理这些反馈，避免零散反馈直接扭曲 skill 的原始职责。

## 核心流程

1. 用户双击桌面脚本打开反馈单。
2. 用户选择或填写被反馈的 skill 名称。
3. 用户填写痛点描述，可选填写场景举例和期望效果。
4. 工具以时间戳命名保存反馈单。
5. 每日自动化归档未处理反馈，生成工作单。
6. AI 根据评价原则判断反馈是否正向、是否符合单一职责、是否需要最小修改。
7. 涉及修改 skill 时，先备份原内容，并最多保留 12 次历史。
8. 自动化完成后，在当日归档目录生成汇报，列出修改点和非修改点。

## 输入项

| 输入项 | 必填 | 说明 |
|------|------|------|
| 被反馈的 skill 技能名称 | 是 | 可从现有 skill 列表选择，也可手动输入 |
| 用户痛点描述 | 是 | 描述实际遇到的问题 |
| 痛点场景举例 | 否 | 补充具体触发场景 |
| 期望效果 | 否 | 描述用户期待的改进结果 |

## 输出项

| 输出项 | 说明 |
|------|------|
| 反馈单 Markdown | 以时间戳命名，先进入待处理目录，处理后进入当日归档目录 |
| 每日工作单 | 汇总当日待处理反馈，供自动化执行 |
| 备份目录 | 修改前保存 skill 原始内容，最多保留 12 次 |
| 每日进化汇报 | 包含修改点和非修改点，并引用对应反馈单 |

## 评价原则

- 理性判断反馈是否为正向反馈，避免 skill 被个人偏好带离原始目标。
- 坚持每个 skill 的单一职责，避免不断膨胀成万能工具。
- 每次修改保持最简，优先改说明、触发条件、边界和 FAQ，谨慎新增复杂流程。

## 技术约束

### 技术栈

| 项目 | 选型 | 说明 |
|------|------|------|
| 语言 | Python 3 标准库 | 避免新增依赖 |
| 表单界面 | Tkinter | Windows 自带 Python 通常可用 |
| 数据格式 | Markdown + YAML-like front matter | 便于人工查看和 AI 处理 |
| 启动方式 | Batch / PowerShell | 支持双击运行和计划任务 |
| 自动化 | Codex cron automation + 本地脚本 | 本地脚本做机械归档，AI 自动化做判断和修改 |

### 目录约定

```
tools/skill_feedback/
├── open_feedback_form.py
├── daily_evolution.py
└── README.md

scripts/
├── OpenSkillFeedback.bat
└── InstallDailySkillEvolutionTask.ps1

skill-feedback/
├── inbox/
├── archive/YYYY-MM-DD/
└── backups/<skill-name>/
```

## 验收标准

- [ ] [001] 双击启动脚本后，用户可以填写并提交反馈单。
- [ ] [002] 反馈单包含 skill 名称、痛点描述、可选场景和可选期望效果。
- [ ] [003] 反馈单文件名包含时间戳，且内容包含创建时间和处理状态。
- [ ] [004] 每日处理脚本能把未处理反馈归档到 `skill-feedback/archive/YYYY-MM-DD/`。
- [ ] [005] 每日归档目录同时包含反馈单、工作单和进化汇报模板。
- [ ] [006] 修改 skill 前可调用备份命令创建备份，并且每个 skill 最多保留 12 次备份。
- [ ] [007] 工作单明确列出三条评价原则，要求区分修改点和非修改点。

## 测试用例

### MT001 反馈单提交

- **测试步骤**：运行 `python tools/skill_feedback/open_feedback_form.py`，填写必填项并提交。
- **预期结果**：`skill-feedback/inbox/feedback_YYYYMMDD_HHMMSS.md` 被创建。
- **验收标准**：[001], [002], [003]

### MT002 每日归档准备

- **测试步骤**：存在至少一份 inbox 反馈时运行 `python tools/skill_feedback/daily_evolution.py prepare`。
- **预期结果**：反馈单被移动到当日 archive 目录，并生成 `evolution-workorder.md` 与 `evolution-report.md`。
- **验收标准**：[004], [005], [007]

### MT003 修改前备份

- **测试步骤**：运行 `python tools/skill_feedback/daily_evolution.py backup --skill auto-skill-creator`。
- **预期结果**：`skill-feedback/backups/auto-skill-creator/<timestamp>/` 中存在原 skill 内容。
- **验收标准**：[006]
