# Issue Tracker 手动执行指南

本文档用于在没有 AI Agent 自动执行时，人工按同一规范维护问题排查记录。文档格式以 `templates/` 为准，创建文件优先调用 `scripts/issue_tracker.py`。

## 固定路径

- 问题记录根目录：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\奔霄-note\问题排查记录\`
- 归档根目录：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\项目集\{项目名}\历史问题\`

## 新建问题记录

在仓库根目录执行：

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py
  --title "问题名称" `
  --project "项目名" `
  --reporter "反馈人" `
  --symptom "问题现象" `
  --priority "P2"
```

脚本会创建：

```text
{yyyy-MM-dd}·{项目名}·{问题描述}/
01_问题详情.md          （含"期望结果"和"验收标准"小节）
02_排查记录.md
03_总结汇报.md
```

对应模板：

- `templates/issue_detail.md`
- `templates/report_placeholder.md`

## 新增每日排查记录

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py
  --query "问题关键词"
```

指定具体目录时：

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py
  --issue-path "C:\...\问题排查记录\20260526-1000.项目名.问题名"
```

对应模板：`templates/daily_log.md`，内容写入 `02_排查记录.md`。

> 同一文件按日期和"阶段 1/2/3..."追加；已有阶段时加 `--append` 生成新阶段，否则脚本返回 `existed=true` 不覆盖。

## 生成总结汇报

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py
  --query "问题关键词"
```

如需预填汇报字段，可追加参数：

```powershell
--project "项目名"
--reporter "反馈人"
--what "问题是什么"
--why "原因"
--how "发生经过"
--validation-result "验证结果"
```

对应模板：`templates/report.md`。

## 查看近期问题

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py
```

## 归档问题

确认问题已解决后执行：

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py
  --query "问题关键词" `
  --project "项目名"
```

脚本会移动整个问题目录到：

```text
C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\项目集\{项目名}\历史问题\
```

## 修改文档格式

不要直接在 `SKILL.md` 或脚本里改 Markdown 样式。只修改 `templates/` 中对应模板：

- 问题详情（含期望结果 + 验收标准）：`templates/issue_detail.md`
- 每日排查（按"阶段 N"追加）：`templates/daily_log.md`
- 总结汇报：`templates/report.md`
- 企微通知：`templates/notification.txt`

模板变量格式为 `{{变量名}}`。脚本未收到的变量会填充为 `待补充。`。

## 测试命令

为了避免写入真实问题目录，可用 `--root` 指定临时目录：

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py --root ".\.claude_tmp\az-issue-tracker-test" create `
  --title "测试问题" `
  --project "测试项目" `
  --reporter "测试人" `
  --symptom "测试现象"
```
