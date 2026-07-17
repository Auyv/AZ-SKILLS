---
name: az-issue-tracker
description: |
  问题排查记录追踪工具。用于创建问题记录、追加每日排查记录、基于已有记录生成 5W2H 汇报材料、列出近期问题、归档已解决问题。

  触发场景：
  - 用户提到“记录问题”“创建问题记录”“问题追踪”“问题报告”“排查问题”
  - 用户提到“开始排查”“继续排查”“今天的排查记录”
  - 用户提到“生成汇报”“总结汇报”“汇报材料”“导出报告”
  - 用户提到“查看问题记录”“最近的问题”“归档问题”
version: 2.5.0
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
    description: 由模板生成的问题详情文件（含"期望结果"和"验收标准"小节）。
    path: "{issue_directory}/01_问题详情.md"
  - name: daily_log_file
    type: file
    description: 由模板生成的排查记录文件；同一文件支持按日期和"阶段 1/2/3..."多次追加。
    path: "{issue_directory}/02_排查记录.md"
  - name: report_file
    type: file
    description: 由模板生成的总结汇报文件。
    path: "{issue_directory}/03_总结汇报.md"
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
az-issue-tracker/
  SKILL.md
  MANUAL.md
  scripts/
    issue_tracker.py
  templates/
    issue_detail.md
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
2. 不要手写 `01_问题详情.md`、`02_排查记录.md`、`03_总结汇报.md` 的结构。
3. 期望结果和验收标准已合并到 `01_问题详情.md` 末尾的两个小节，不再单独生成 `02_期望结果.md`。
4. 排查记录集中写入 `02_排查记录.md`，同一文件内按日期和"阶段 1/2/3..."追加；首次写入阶段由 daily 子命令自动完成，后续追加需显式加 `--append`，脚本会读取现有阶段号 +1 并写入当前时间戳。
5. 如果用户只要求调整格式，直接修改 `templates/`，再运行脚本验证。
6. 读取和写入中文文件统一使用 UTF-8。
7. 脚本返回 JSON。回复用户时提取关键字段，不要把完整 JSON 原样倾倒给用户。

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
ls -la "/home/jpz/.hermes/skills/note-taking/az-issue-tracker-execution/"
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
| 跳过 skill_view 直接创建 | 沿用旧习惯，未加载技能文档 | 必须先加载 skill_view(az-issue-tracker) |
| 仍创建 `03_排查记录/` 子目录 | 沿用了旧规范 | 按 `01_问题详情.md`、`02_排查记录.md`、`03_总结汇报.md` 三文件结构 |
| Windows 绝对路径被当作相对路径 | WSL 下 `C:\...` 路径被 pathlib 视为相对路径 | WSL 下使用 /mnt/c/ 前缀的绝对路径 |
| 脚本显示成功但文件不在预期目录 | 上一条错误的直接后果 | 先检查文件实际位置，再手动 mv 到正确路径 |
| `--root` 报 unrecognized arguments | argparse 子解析器要求顶层参数必须在子命令**之前** | 正确顺序：`issue_tracker.py --root <path> create --title ...`；`--root` 写在 `create` 之后会被 argparse 拒绝 |
| `archive` 报 success 但归档目录不在 `项目集/{项目}/历史问题/`，文件"失踪" | `DEFAULT_ARCHIVE_ROOT = Path(r"C:\...")` 是 Windows 反斜杠路径，WSL 下 `pathlib` 把它当**相对路径**拼到 CWD；`shutil.move` 实际把目录移到 `{CWD}/C:\...\项目集/{项目}/历史问题/...` 这种畸形位置，脚本仍然"success"返回 | WSL 下执行 archive **必须**显式传 `--archive-root` 为 WSL 格式绝对路径，例：`--archive-root "/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/项目集"`；事后用 `find /mnt/c/Users -type d -name "{项目名}"` 验证文件位置，搜索"成功"后的实际目标目录 |
| Windows 原生 Python 跑 archive 时 `--root` / `--archive-root` 报 "unrecognized arguments" | argparse 的 `--root`、`--archive-root` 是**顶层参数**，必须在子命令（`create`/`daily`/`report`/`list`/`archive`）**之前**；写在子命令之后会被 argparse 拒绝 — 即使补对了路径也跑不通 | 严格按顺序：`issue_tracker.py --root <root> --archive-root <archive_root> archive --issue-path <path> --project <name>` |
| 项目名大小写 / 中英文命名想规范化 | `create` 时 `--project` 决定了目录名后缀，事后改名只能用文件系统操作（`mv` / PowerShell `Rename-Item`），没有脚本命令支持 | 用户要求改名时直接用 `mv "<root>/<旧名>" "<root>/<新名>"`，三个文件随目录一起搬。**改名前必须先和用户确认一次新名**（命名规范、是否保留日期），不要自作主张 |
| 用户想把工单/外部沟通链接固化到问题详情 | `templates/issue_detail.md` 没有"对外沟通"字段；脚本也不会写 | 用 `patch` 工具手动往 `01_问题详情.md` 追加 `## 对外沟通` 小节（包含工单号、链接、联系人、状态），不要重写整个文件。注意：阶段 2 写过的外部链接也要回填到 01，保持单一来源 |
| 想抓 Jira Service Desk / 内部系统工单正文当排查材料 | 这些平台通常需要登录才能渲染；浏览器工具会卡在登录页 | 不要死磕抓正文。直接问用户复制正文贴过来；或者用 `patch` 先把"工单号 + 链接 + 已发问题描述"作为最小集写入，等用户拿到正文再补一个阶段 3 |
| 归档后目录里 `03_总结汇报.md` 还是 `<在此处填写>` 模板 | 跳过了"归档前补全三件套"步骤，直接 archive | 归档前必须先把 02 追加结论阶段、01 改状态、03 按 5W2H 填完整，再 archive。详见下文"归档前的三件套补全流程" |
| 用户列出"原因 A + 方案 1、方案 2"，但 A 实际是单根因，方案 1/2 是同一根因下的两处本地修改 | Agent 倾向把"两个修改点"自动套用"双重根因 + 两个并列方案"的框架，把它们写得像平行独立的处置路径 | **不要自动升级为"双重根因 + 两个并列方案"**。先就"是单根因还是多根因、是并列方案还是同一根因下的多个修改点"追问一次。错把"两处协同修改"叙述成"两个并列方案"，会让 5W2H、解决方案、经验沉淀全部失真，后续回归和跨项目借鉴也会跑偏 |
| 归档完成后用户又反馈新信息（例如模组官方答复、测试用例修订完毕），需要补一个阶段 | `issue_tracker.py daily --query ...` 只搜索 `问题排查记录/` 根目录，归档后的目录已经不在那里，脚本会返回 `ERR_ISSUE_NOT_FOUND` | 归档后追加排查记录只能用 `patch` 工具直接编辑归档目录下的 `02_排查记录.md`，自己拼"## YYYY-MM-DD 阶段 N（HH:MM · 备注）"小节末尾追加。同步要 patch `01_问题详情.md`（状态/验收标准/对外沟通）和 `03_总结汇报.md`。详见下文"归档后追加内容"小节 |

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
python .\skills\az-issue-tracker\scripts\issue_tracker.py create `
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
    01_问题详情.md      （含期望结果 + 验收标准）
    02_排查记录.md
    03_总结汇报.md
```

并返回企微通知文案。默认只生成文案，不直接发送。

### 项目名约定

- 目录名后缀是 `--project` 的原样透传，没有规范化。
- 用户给的写法如果含中文后缀或大小写不规范（如 `hqt401-k01东风柳汽`），**先问一下要不要规范化**（典型规范：`HQT401-K01`，去掉中文项目名后缀、全大写型号），因为改名要走文件系统。
- 同一项目的新问题**沿用既有目录名的项目字段**，不要每条记录各写各的；如有疑问先 `list` 看一下同项目历史命名。

## add_daily_log

优先使用用户给出的 `--issue-path`。没有路径时使用 `--query` 在问题记录根目录中匹配。

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py daily `
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
--append            # 02_排查记录.md 已有阶段时追加新阶段（阶段号 = 现有最大 + 1）
```

### 行为说明

- **首次写入阶段**：`02_排查记录.md` 仍为占位内容时，写入"{date} 阶段 1（HH:MM）"小节。
- **追加（--append）**：`02_排查记录.md` 已有阶段时，扫描现有"## 阶段 N"标题，取最大 N + 1 作为新阶段号，在文件末尾追加"阶段 N+1（HH:MM）"小节。
- **未加 --append 且文件已存在**：脚本返回 `existed=true` 并提示加 `--append`，**不写入新内容，但仍返回 `status: success`**。这是个会误导 Agent 的双重信号（看起来成功，实际啥也没干）。**判定标准**：返回值里 `phase` 是新数字、`phase_timestamp` 存在，才算真正写入；`existed:true` 出现时默认认为没写。

如果 `02_排查记录.md` 需要整体重新生成（罕见），请先手动备份并删除旧文件再调用 daily 子命令。

## generate_report

使用固定汇报模板生成 `03_总结汇报.md`：

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py report `
  --query "问题关键词"
```

脚本会检查 `01_问题详情.md`、`02_排查记录.md`。兼容旧记录中的 `03_排查记录/*.md` 和 `04_总结汇报.md`；当前版本不会自动总结全文内容，只保证汇报结构统一；需要 AI 根据已有记录提炼事实时，先读取记录，再通过参数传入 5W2H 字段或生成后补充内容。

> 旧规范的 `02_期望结果.md` 已合并到 `01_问题详情.md`；如果问题目录里仍残留 02 文件，脚本会忽略。

不要直接改写汇报结构；如要改结构，修改 `templates/report.md`。

## list_issues

```powershell
python .\skills\az-issue-tracker\scripts\issue_tracker.py list --limit 10
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
python .\skills\az-issue-tracker\scripts\issue_tracker.py archive `
  --query "问题关键词" `
  --project "项目名"
```

脚本会移动整个问题目录到：

```text
C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\项目集\{项目名}\历史问题\
```

如果归档目标已存在，不覆盖。

### 归档前的"三件套补全"流程（2026-07-16 沉淀）

归档前绝大多数问题都需要把 01 / 02 / 03 三个文件从"占位 + 阶段 1"补成完整闭环，否则归档后目录里会残留 `<在此处填写>` 模板占位符（典型表现：03_总结汇报.md 仍是 `templates/report_placeholder.md` 的内容）。

**标准四步走**：

1. 追加最后一个排查阶段（结论阶段），如果 02 还只有"阶段 1"
   ```powershell
   python .\skills\az-issue-tracker\scripts\issue_tracker.py `
     --root "<root>" --archive-root "<archive_root>" `
     daily --query "<问题关键词>" `
     --date "<归档日>" `
     --investigation-goal "..." --analysis-process "..." `
     --possible-cause "..." --verification-method "..." `
     --verification-result "..." --conclusion "..." --next-plan "..." `
     --append
   ```

2. **重写 `01_问题详情.md`**：状态从"待排查"改为"已解决"（或对应状态），补齐"初步判断 / 影响范围"，验收标准全部打勾。直接用 write_file 整体覆盖，保留模板里的"期望结果 + 验收标准"小节结构。

3. **重写 `03_总结汇报.md`**：按 `templates/report.md` 的 5W2H 字段填完整，不要留 `<在此处填写>` 占位符。

4. 最后才执行 archive。

### "误报 / 设计预期 / 无需改软件" 类问题的写法

当结论是"测试用例误判"或"设计如此"时，5W2H 表要明确写出**双轨处置**，避免给后来人留"为啥不修代码"的疑问：

- **How Much**：代价 = 软件侧 0 + 文档侧 N 条用例修订 + 工单侧关闭
- **解决方案**：
  - 临时方案：通知反馈人关闭误报工单 / 当面解释清楚
  - 永久方案：测试团队修订用例剔除该窗口；列出修订后的判定条件
- **后续跟踪**：跨项目检查同类用例是否含类似误判（避免下次又冒头）

具体案例参考 `references/archive-completion-recipes.md`（覆盖误报、真 BUG、挂起三种典型场景，含归档前自检清单）。

### 归档后追加内容（补丁场景）

归档只是 `shutil.move`，目录已经离开 `问题排查记录/` 根目录。**脚本对归档目录不可见**，所以归档后再补内容要走手工 `patch` 路线。

**典型触发**：

- 模组/平台工单在归档后给出官方答复（例如 `has_addr` 语义确认），需要补一个澄清阶段
- 测试用例修订完毕 / 验证用例通过，需要补一个验证阶段
- 跨项目核对结论出来，需要把适用范围 / 升级清单补全

**操作步骤**：

1. `daily --query "<关键词>"` 会返回 `ERR_ISSUE_NOT_FOUND`（脚本只搜 `问题排查记录/`），**不要怀疑是 bug**；直接转人工 `patch`
2. 用 `patch` 工具在归档目录下追加 `02_排查记录.md` 的新阶段，手写小节标题格式：
   ```markdown
   ---

   ## YYYY-MM-DD 阶段 N（HH:MM · 备注关键词）

   **排查目标**
   ...

   **分析过程**
   ...
   ```
   阶段号取自现有最大阶段 + 1；时间戳用当前时间
3. 同步 `patch` `01_问题详情.md`：状态、验收标准、对外沟通小节（保持单一来源）
4. 同步 `patch` `03_总结汇报.md`：5W2H 表的 Why / 解决方案 / 经验沉淀 / 后续跟踪
5. 不要再次调用 `archive` —— 目录已经在 `项目集/{项目}/历史问题/` 里，不需要再移动

## 模板维护规则

- `templates/issue_detail.md`：问题详情格式（含期望结果 + 验收标准）
- `templates/daily_log.md`：排查阶段格式（写入 `02_排查记录.md`，同一文件按日期和"阶段 N"追加）
- `templates/daily_log_placeholder.md`：新建记录时的排查记录占位文件
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
