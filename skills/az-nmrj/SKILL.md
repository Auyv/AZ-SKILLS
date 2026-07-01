---
name: az-nmrj
description: |
  nmrj 自然语言操作技能。用于把用户的自然语言请求转换为 nmrj CLI 操作，覆盖初始化、配置、任务、日报、附件、数据备份/导出/导入、AI 排期建议和交互模式。

  触发场景：
  - 用户提到 nmrj 并希望“登记任务”“创建任务”“写日报”“补日志”“更新进度”
  - 用户要求“查任务”“列日报”“导出日报”“备份数据”“导入数据”“管理附件”
  - 用户用自然语言描述 nmrj 相关操作，希望 Agent 代为执行
  - 当前对话需要把已完成工作沉淀到 nmrj 的任务或日报中
version: 1.1.0
inputs:
  required:
    - name: user_intent
      type: string
      description: 用户本次要执行的 nmrj 操作，例如登记任务、填写日报、备份数据、添加附件。
      validation: 非空
  optional:
    - name: action
      type: string
      description: 归一化动作，例如 init/config_get/task_add/report_add/attach_add/data_backup/plan_suggest。
      default: infer_from_user_intent
    - name: project
      type: string
      description: 项目名称；登记任务和任务筛选时常用。
      default: null
    - name: main
      type: string
      description: 主任务名称；登记任务时必需。
      default: null
    - name: sub
      type: string
      description: 子任务名称；登记任务时必需。
      default: null
    - name: task_id
      type: string
      description: nmrj 任务 ID 或唯一前缀；日报、详情、更新、附件操作时常用。
      default: null
    - name: log
      type: string
      description: 日报日志内容。
      default: null
    - name: hours
      type: integer
      description: 工时，单位小时。
      default: null
    - name: progress
      type: integer
      description: 任务进度，0-100，只允许递增。
      default: null
    - name: file_path
      type: string
      description: 附件、导入、导出或备份文件路径。
      default: null
    - name: date
      type: string
      description: 日期，格式 YYYY-MM-DD；缺省时根据 nmrj 命令默认值处理。
      default: null
outputs:
  - name: normalized_action
    type: string
    description: 从自然语言归一化出的 nmrj 动作。
  - name: command
    type: array
    description: 最终执行或建议执行的 nmrj 命令参数数组。
  - name: cli_result
    type: object
    description: nmrj 执行结果，包含 stdout、stderr、exit_code。
  - name: follow_up_question
    type: string
    description: 信息不足时提出的最少补充问题。
  - name: summary
    type: string
    description: 面向用户的操作结果摘要。
errors:
  - code: ERR_MISSING_REQUIRED_INFO
    message: 执行目标操作所需字段不足。
    handling: 只追问缺失字段；不要编造项目、任务 ID、工时、进度或路径。
  - code: ERR_NMRJ_NOT_INITIALIZED
    message: nmrj 尚未初始化或配置缺失。
    handling: 提示执行 nmrj init --data-dir <path>，或先运行 config get 检查配置。
  - code: ERR_TASK_NOT_FOUND
    message: 指定任务 ID 不存在或前缀不唯一。
    handling: 运行 task list 辅助用户选择正确任务，再继续操作。
  - code: ERR_INVALID_PROGRESS
    message: 进度格式错误、超过 100，或低于当前任务进度。
    handling: 读取任务详情确认当前进度，要求用户给出不低于当前值的新进度。
  - code: ERR_INVALID_DATE
    message: 日期格式不是 YYYY-MM-DD。
    handling: 将相对日期换算成明确日期；无法判断时询问用户。
  - code: ERR_FILE_NOT_FOUND
    message: 用户指定的附件、导入文件或输出目录不存在。
    handling: 提示用户确认路径；不要自行创建不明确的上级目录。
  - code: ERR_DESTRUCTIVE_CONFIRMATION_REQUIRED
    message: 操作会覆盖、导入、删除或重置数据。
    handling: 未得到用户明确确认时，不传 --force、--yes 或 --overwrite。
  - code: ERR_NMRJ_COMMAND_FAILED
    message: nmrj 命令执行失败。
    handling: 展示关键 stderr/stdout，并说明下一步可恢复操作。
---

# az-nmrj

## 目标

让用户用自然语言完成 nmrj 的全部本地 CLI 功能。Agent 负责理解意图、补齐必要字段、选择正确子命令、执行 nmrj，并用简短摘要反馈结果。

nmrj 项目路径：`C:\JPZ_Workspace\AZ_TOOLS\nmrj`。

## 文件结构

```text
az-nmrj/
  SKILL.md
  MANUAL.md
  scripts/
    nmrj_cli.py
  evals/
    evals.json
```

`scripts/nmrj_cli.py` 是 JSON 包装器，避免手写命令引号和编码问题。自然语言理解、补问、风险判断由 Agent 执行。

## 使用边界

使用本技能处理 nmrj 数据操作。不要用它替代 nmrj 项目开发、代码修复或 SDD 流程；如果用户是在开发 nmrj 本身，只有当用户明确要求“记录到 nmrj”时才登记任务或日报。

## 动作映射

| 用户意图 | nmrj 命令 | 包装脚本 action |
| --- | --- | --- |
| 初始化数据目录 | `nmrj init` | `init` |
| 查看配置 | `nmrj config get` | `config-get` |
| 修改配置 | `nmrj config set` | `config-set` |
| 新建任务 | `nmrj task add` | `task-add` |
| 查询任务列表 | `nmrj task list` | `task-list` |
| 查看任务详情 | `nmrj task get` | `task-get` |
| 更新任务 | `nmrj task update` | `task-update` |
| 删除任务 | `nmrj task delete` | `task-delete` |
| 新增日报 | `nmrj report add` | `report-add` |
| 查询日报 | `nmrj report list` | `report-list` |
| 导出日报 | `nmrj report export` | `report-export` |
| 添加附件 | `nmrj attach add` | `attach-add` |
| 查看附件 | `nmrj attach list` | `attach-list` |
| 打开附件目录 | `nmrj attach open` | `attach-open` |
| 删除附件 | `nmrj attach delete` | `attach-delete` |
| 备份数据 | `nmrj data backup` | `data-backup` |
| 导出完整数据 | `nmrj data export` | `data-export` |
| 导入数据 | `nmrj data import` | `data-import` |
| AI 排期建议 | `nmrj plan suggest` | `plan-suggest` |
| 交互模式 | `nmrj tui` | 直接提示用户运行；Agent 不主动进入长期交互会话 |

## 执行规则

1. 先归一化动作，再检查必填字段。
2. 优先用包装脚本执行命令；它会返回 JSON，便于摘要和错误处理。
3. 缺字段时只问必要问题。
4. 破坏性操作必须确认：`init --force`、`task delete`、`attach delete`、`data import -y`、覆盖导出文件等。
5. 写日报前必须确认任务 ID、日志、工时、进度。
6. 登记任务前必须确认项目、主任务、子任务。
7. 涉及路径时保留用户原始路径，不自行猜测文件名。
8. 进度必须是 0-100 的整数；如果可能，先 `task-get` 获取当前进度。
9. 日期使用 `YYYY-MM-DD`。相对日期按当前系统日期换算，并在回复中写明换算结果。
10. 执行后用简短中文总结结果，必要时引用关键 stdout/stderr。

## 常见自然语言处理

- “帮我记个任务”：需要项目、主任务、子任务；缺哪个问哪个。
- “刚才这个工作记日报”：从对话总结日志，但仍要确认任务 ID、工时、进度。
- “完成了”：不能自动等于 100，除非用户明确说“进度到 100%/已完成”；否则询问。
- “昨天”：换算成明确日期再传 `--date`。
- “导出到桌面”：如果路径不明确，询问完整输出文件路径。
- “删掉这个任务/导入这个备份”：先复述影响并等待明确确认。

## 包装脚本示例

```powershell
python .\scripts\nmrj_cli.py config-get
python .\scripts\nmrj_cli.py task-add --project nmrj --main "文档" --sub "补 README" --estimate 2
python .\scripts\nmrj_cli.py report-add --task 2026053101 --log "完成 README" --hours 2 --progress 100
python .\scripts\nmrj_cli.py attach-add --task 2026053101 --file D:\docs\readme.md
python .\scripts\nmrj_cli.py data-backup
```

脚本输出：

```json
{
  "status": "success",
  "timestamp": "2026-05-31T18:00:00+08:00",
  "command": ["nmrj", "task", "list", "-o", "json"],
  "exit_code": 0,
  "stdout": "...",
  "stderr": ""
}
```
