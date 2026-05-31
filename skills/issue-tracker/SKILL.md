---
name: issue-tracker
description: |
  问题排查记录追踪工具。用于创建问题记录、追加每日排查记录、基于已有记录生成 5W2H 汇报材料、列出近期问题、归档已解决问题。

  触发场景：
  - 用户提到“记录问题”“创建问题记录”“问题追踪”“问题报告”“排查问题”
  - 用户提到“开始排查”“继续排查”“今天的排查记录”
  - 用户提到“生成汇报”“总结汇报”“汇报材料”“导出报告”
  - 用户提到“查看问题记录”“最近的问题”“归档问题”
version: 2.1.0
inputs:
  required:
    - name: user_intent
      type: string
      description: 用户本次要执行的动作，例如新建问题、继续排查、生成汇报、归档。
      validation: 非空
  optional:
    - name: issue_title
      type: string
      description: 问题名称或一句话简述。
      default: null
    - name: project_name
      type: string
      description: 所属项目或产品名称。
      default: null
    - name: reporter
      type: string
      description: 反馈人或问题报告人。
      default: null
    - name: symptom
      type: string
      description: 问题现象、复现描述或影响说明。
      default: null
    - name: priority
      type: string
      description: P0/P1/P2/P3。
      default: P2
    - name: issue_path
      type: string
      description: 已有问题记录目录路径。
      default: null
    - name: date
      type: string
      description: 排查记录日期，格式 YYYY-MM-DD；缺省时使用系统日期。
      default: today
outputs:
  - name: issue_directory
    type: directory
    description: 问题记录目录。
    path: "C:\\JPZ_Workspace\\笔记仓库-jpz\\hq-jpz\\奔霄-note\\问题排查记录\\{yyyy-MM-dd}·{project}·{title}\\"
  - name: detail_file
    type: file
    description: 由模板生成的问题详情文件。
    path: "{issue_directory}/01_问题详情.md"
  - name: expected_file
    type: file
    description: 由模板生成的期望结果文件。
    path: "{issue_directory}/02_期望结果.md"
  - name: daily_log_file
    type: file
    description: 由模板生成的每日排查记录。
    path: "{issue_directory}/03_排查记录/{date}.md"
  - name: report_file
    type: file
    description: 由模板生成的总结汇报文件。
    path: "{issue_directory}/04_总结汇报.md"
errors:
  - code: ERR_MISSING_REQUIRED_INFO
    message: 创建或更新记录所需信息不足。
    handling: 只追问缺失字段；单次最多问 5 个问题。
  - code: ERR_ISSUE_NOT_FOUND
    message: 未找到指定问题记录目录。
    handling: 运行 list 动作列出最近记录，请用户选择。
  - code: ERR_AMBIGUOUS_ISSUE
    message: 多个问题记录匹配用户描述。
    handling: 列出候选项并要求用户确认。
  - code: ERR_DUPLICATE_ISSUE
    message: 新建问题目录、文件或归档目标已存在。
    handling: 不覆盖；询问是复用已有记录、追加后缀创建新记录，还是取消。
  - code: ERR_TEMPLATE_NOT_FOUND
    message: 脚本引用的模板文件不存在。
    handling: 停止写入，提示检查 templates/ 目录。
  - code: ERR_REPORT_SOURCE_EMPTY
    message: 生成汇报时缺少问题详情或排查记录内容。
    handling: 生成可填写的汇报草稿，并明确标记待补充项。
  - code: ERR_PERMISSION_DENIED
    message: 目标目录无法写入或移动。
    handling: 停止文件操作，提示用户检查权限或更换目录。
---

# Issue Tracker

## 目标

把临时问题反馈变成可追踪、可汇报、可归档的问题排查记录。所有标准 Markdown 文件必须通过脚本和模板生成，避免不同 Agent 手写造成格式漂移。

## 目录职责

```text
issue-tracker/
  SKILL.md
  MANUAL.md
  scripts/
    issue_tracker.py
  templates/
    issue_detail.md
    expected_result.md
    daily_log.md
    report.md
    report_placeholder.md
    notification.txt
  evals/
    evals.json
```

模板是文档格式的单一来源。修改文档样式时优先改 `templates/`，不要在 `SKILL.md` 中复制一份新模板。

## 固定路径

- 问题记录根目录：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\奔霄-note\问题排查记录\`
- 归档根目录：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\项目集\{对应项目}\历史问题\`
- 通知负责人：工号 `101706`，姓名 `靳普诏`

如果当前环境无法访问上述路径，先说明 `ERR_PERMISSION_DENIED` 或路径不存在，不要改写到未确认的位置。

## 执行原则

1. 创建、每日排查、生成汇报、列出记录、归档都优先调用 `scripts/issue_tracker.py`。
2. 不要手写 `01_问题详情.md`、`02_期望结果.md`、`03_排查记录/{date}.md`、`04_总结汇报.md` 的结构。
3. 如果用户只要求调整格式，直接修改 `templates/`，再运行脚本验证。
4. 读取和写入中文文件统一使用 UTF-8。
5. 脚本返回 JSON。回复用户时提取关键字段，不要把完整 JSON 原样倾倒给用户。

---

## WSL 路径陷阱（重要）

`issue_tracker.py` 中 `DEFAULT_ROOT` 硬编码为 Windows 反斜线路径 `C:\\JPZ_Workspace\\...`。在 WSL 环境下运行 Python 时，此类路径会被 Python 的 `pathlib.Path` 当作**相对路径**处理，导致文件被写到技能目录内部而非知识库。

**表现**：创建的文件在 `/home/jpz/.hermes/skills/.../C:\JPZ_Workspace\...` 下，而不是知识库中。

**预防**：在 WSL 下执行时，必须使用 `--root` 参数传入 WSL 格式绝对路径：
```bash
python3 scripts/issue_tracker.py create \
  --root "/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/奔霄-note/问题排查记录" \
  ...
```

**应急处理**：若文件被错误写入技能目录，用以下命令救出：
```bash
# 1. 找到错误路径
ls -la "/home/jpz/.hermes/skills/note-taking/issue-tracker-execution/"
# 2. 移动到正确位置
cp -r "<错误路径>/<问题目录>" \
      "/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/奔霄-note/问题排查记录/"

# 3. 清理错误位置
rm -rf "<错误路径>"
```

---

## 常见执行错误

| 错误 | 原因 | 正确做法 |
|------|------|----------|
| 创建单一 AZ排查记录.md 文件 | 未按规范执行，模仿了旧格式 | 严格使用 scripts/issue_tracker.py |
| 跳过 skill_view 直接创建 | 沿用旧习惯，未加载技能文档 | 必须先加载 skill_view(issue-tracker) |
| 目录结构缺少排查记录子目录 | 未仔细阅读规范 | 按 01/02/03/04 固定结构 |
| Windows 绝对路径被当作相对路径 | WSL 下 `C:\...` 路径被 pathlib 视为相对路径 | WSL 下使用 /mnt/c/ 前缀的绝对路径 |
| 脚本显示成功但文件不在预期目录 | 上一条错误的直接后果 | 先检查文件实际位置，再手动 mv 到正确路径 |

## 动作判定

| 动作 | 触发表达 | 脚本命令 |
| --- | --- | --- |
| `create_issue` | 记录问题、创建问题记录、问题反馈、问题报告 | `python scripts/issue_tracker.py create ...` |
| `add_daily_log` | 开始排查、继续排查、今天的排查记录 | `python scripts/issue_tracker.py daily ...` |
| `generate_report` | 生成汇报、总结汇报、汇报材料、导出报告 | `python scripts/issue_tracker.py report ...` |
| `list_issues` | 查看记录、最近问题、有哪些问题 | `python scripts/issue_tracker.py list` |
| `archive_issue` | 归档问题、已解决、移动到历史问题 | `python scripts/issue_tracker.py archive ...` |

如果动作不明确，先用一句话确认目标动作。不要同时执行多个会写文件的动作，除非用户明确要求。

## create_issue

### 必需信息

- 问题名称或简述：`--title`
- 所属项目：`--project`
- 反馈人：`--reporter`
- 问题现象：`--symptom`
- 优先级：`--priority`，缺省为 `P2`

缺少信息时追问缺失项。用户一次性提供充分信息时不要重复追问。

### 命令格式

```powershell
python .\skills\issue-tracker\scripts\issue_tracker.py create `
  --title "问题名称" `
  --project "项目名" `
  --reporter "反馈人" `
  --symptom "问题现象" `
  --priority "P2"
```

脚本会创建：

```text
{root}/
  {yyyy-MM-dd}·{项目名}·{问题描述}/
    01_问题详情.md
    02_期望结果.md
    03_排查记录/
    04_总结汇报.md
```

并返回企微通知文案。默认只生成文案，不直接发送。

## add_daily_log

优先使用用户给出的 `--issue-path`。没有路径时使用 `--query` 在问题记录根目录中匹配。

```powershell
python .\skills\issue-tracker\scripts\issue_tracker.py daily `
  --query "问题关键词"
```

可选传入：

```powershell
--date "2026-05-26"
--investigation-goal "今天要确认的问题"
--analysis-process "分析过程"
--possible-cause "可能原因"
--verification-method "验证方法"
--verification-result "验证结果"
--conclusion "结论"
--next-plan "下一步计划"
```

如果当天文件已存在，脚本不会覆盖，会返回现有路径。

## generate_report

使用固定汇报模板生成 `04_总结汇报.md`：

```powershell
python .\skills\issue-tracker\scripts\issue_tracker.py report `
  --query "问题关键词"
```

脚本会检查 `01_问题详情.md`、`02_期望结果.md`、`03_排查记录/*.md`。当前版本不会自动总结全文内容，只保证汇报结构统一；需要 AI 根据已有记录提炼事实时，先读取记录，再通过参数传入 5W2H 字段或生成后补充内容。

不要直接改写汇报结构；如要改结构，修改 `templates/report.md`。

## list_issues

```powershell
python .\skills\issue-tracker\scripts\issue_tracker.py list --limit 10
```

只读目录元数据，返回：

- 目录名
- 路径
- 最后修改时间
- 是否已有总结汇报
- 是否有当天排查记录

## archive_issue

归档前必须确认问题已解决，并能确定唯一问题目录和项目名。

```powershell
python .\skills\issue-tracker\scripts\issue_tracker.py archive `
  --query "问题关键词" `
  --project "项目名"
```

脚本会移动整个问题目录到：

```text
C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\项目集\{项目名}\历史问题\
```

如果归档目标已存在，不覆盖。

## 模板维护规则

- `templates/issue_detail.md`：问题详情格式
- `templates/expected_result.md`：期望结果格式
- `templates/daily_log.md`：每日排查记录格式
- `templates/report.md`：总结汇报格式
- `templates/report_placeholder.md`：新建记录时的汇报占位文件
- `templates/notification.txt`：企微通知文案

模板变量使用 `{{变量名}}`。脚本未收到的变量会替换为 `待补充。`。

## 完成回复格式

执行成功后，回复包含：

- 已执行动作
- 创建或更新的文件路径
- 是否有待补充项
- 下一步建议

执行失败或信息不足时，回复包含：

- 错误码
- 缺失或冲突点
- 用户下一步需要提供的信息
