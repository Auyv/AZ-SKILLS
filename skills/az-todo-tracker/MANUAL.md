# Todo Tracker 操作手册(手动执行版)

> 本手册面向**没有 AI Agent**、需要直接用命令行操作的人。
> 所有命令在 Windows PowerShell 环境下给出;WSL 用户把 `python` 换成 `python3`,并把 `C:\...` 换成 `/mnt/c/...`。

## 1. 路径速查

| 用途 | Windows 路径 | WSL 路径 |
| --- | --- | --- |
| 任务待办根目录 | `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\奔霄-note\任务待办` | `/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/奔霄-note/任务待办` |
| 归档根目录 | `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\奔霄-note\任务归档` | `/mnt/c/JPZ_Workspace/笔记仓库-jpz/hq-jpz/奔霄-note/任务归档` |
| 技能脚本 | `<skill_dir>\scripts\task_tracker.py` | 同上 |

## 2. 六大子命令速查

| 子命令 | 用途 | 关键参数 |
| --- | --- | --- |
| `create` | 新建任务 | `--title` `--project` `--owner` |
| `progress` | 追加推进记录 | `--query` 或 `--task-path` `--action-text` |
| `update` | 改状态/优先级/截止日期 | `--query` `--status` |
| `report` | 生成总结汇报 | `--query` |
| `list` | 列出近期任务 | `--limit` |
| `archive` | 归档任务 | `--query` |

## 3. 标准流程

### 3.1 新建任务

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py create `
  --title "MCUD 性能测试 — 搭建测试服务进程" `
  --project "MCUD" `
  --owner "靳普诏(101706)" `
  --priority "中" `
  --status "待启动" `
  --background "MCUD 接口稳定性需验证" `
  --goal "搭建测试服务进程,模拟实车环境,产出性能/稳定性报告" `
  --blocker-1 "MCUD 协议文档位置未知" `
  --trigger-source "主人 2026-06-26 企微对话"
```

**输出**:在 `任务待办/<日期>·MCUD·MCUD 性能测试 — 搭建测试服务进程/` 下生成 `README.md`、`02_推进记录.md`、`03_总结汇报.md` 三个文件。

### 3.2 启动任务(状态:待启动 → 进行中)

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py update `
  --query "MCUD 性能测试" `
  --status "进行中" `
  --append-progress-row "正式启动任务,等主人补充 MCUD 资料"
```

### 3.3 追加推进记录

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py progress `
  --query "MCUD 性能测试" `
  --action-text "完成 MCUD 接口协议分析" `
  --result "梳理出 5 个核心接口" `
  --next-plan "搭建测试服务进程骨架"
```

**注意**:第二次调用 `progress` 必须加 `--append`,否则脚本会拒绝覆盖。

### 3.4 生成总结汇报

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py report `
  --query "MCUD 性能测试" `
  --goal "搭建测试服务进程" `
  --lessons-learned "经验:在 WSL 下必须用 /mnt/c/ 前缀传 --root"
```

### 3.5 归档任务

```powershell
# 1. 先把状态改为已完成
python .\skills\az-todo-tracker\scripts\task_tracker.py update `
  --query "MCUD 性能测试" `
  --status "已完成" `
  --append-progress-row "所有任务已交付,准备归档"

# 2. 归档
python .\skills\az-todo-tracker\scripts\task_tracker.py archive `
  --query "MCUD 性能测试"
```

**输出**:整个任务目录被移动到 `任务归档/`,原位置不再保留。

### 3.6 查看所有待办

```powershell
python .\skills\az-todo-tracker\scripts\task_tracker.py list --limit 20
```

## 4. 常见错误排查

| 报错 | 原因 | 解决 |
| --- | --- | --- |
| `ERR_MISSING_REQUIRED_INFO` | 缺少必填参数 | 重新传入 `--title` / `--project` / `--owner` |
| `ERR_TASK_NOT_FOUND` | `--query` 模糊匹配不到任务 | 用 `list` 列出所有任务,确认目录名 |
| `ERR_AMBIGUOUS_TASK` | `--query` 命中多个目录 | 用更精确的关键词,或直接传 `--task-path` |
| `ERR_DUPLICATE_TASK` | 目标目录已存在 | 改用 `update` 或 `progress`,不要 `create` |
| `ERR_INVALID_STATUS` | 状态值不在 6 选 1 内 | 改用:待启动/进行中/已完成/已挂起/已取消/阻塞 |
| `ERR_TEMPLATE_NOT_FOUND` | templates 目录缺失 | 检查技能目录完整性 |
| `ERR_PERMISSION_DENIED` | 目标路径无写权限 | 用 `--root` 切到有写权限的目录 |
| `unrecognized arguments: --root` | `--root` 写在子命令之后 | 改成 `task_tracker.py --root X create --title Y` |

## 5. 目录命名规范

任务目录命名格式(由脚本自动生成):

```
<YYYY-MM-DD>·<项目编号>·<任务名>/
```

- 日期:任务创建日(YYYY-MM-DD)
- 项目编号:简短大写英文/数字(如 `MCUD`、`YLB04_A6`)
- 任务名:一句话简述,禁用 `<>:"/\|?*` 等非法字符(脚本会自动替换为 `-`)

## 6. 与 issue-tracker 配合使用

| 任务阶段 | 用 issue-tracker | 用 az-todo-tracker |
| --- | --- | --- |
| 立项 | | ✓ |
| 推进 | | ✓ |
| 出问题 | ✓(在 todo 内部嵌入 issue) | |
| 出问题修复 | ✓ | |
| 出汇报 | | ✓(任务汇报) / ✓(问题 5W2H) |
| 归档 | ✓(移到项目集/历史问题) | ✓(移到任务归档) |

**典型场景**:任务 A 在推进过程中发现了一个反复出现的 BUG,这时应该:
1. 在 A 的 `02_推进记录.md` 里写一行"发现 BUG,已记录为 issue #2026-06-26·WIT803-111·..."
2. 用 issue-tracker 单独记录这个 BUG
3. BUG 修完后,在 A 的 `02_推进记录.md` 里写"Bug 已修复,继续推进"
