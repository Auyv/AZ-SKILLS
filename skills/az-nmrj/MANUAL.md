# az-nmrj 手动执行指南

本指南用于在没有 AI Agent 自动执行时，按同一规范用自然语言拆解 nmrj 操作并手动执行。

## 路径

- nmrj 项目：`C:\JPZ_Workspace\AZ_TOOLS\nmrj`
- 技能目录：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\skills\az-nmrj`
- 包装脚本：`scripts\nmrj_cli.py`

## 前置检查

```powershell
cd C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\skills\az-nmrj
python .\scripts\nmrj_cli.py config-get
```

如果未初始化：

```powershell
python .\scripts\nmrj_cli.py init --data-dir D:\nmrj-data
```

## 配置

```powershell
python .\scripts\nmrj_cli.py config-get
python .\scripts\nmrj_cli.py config-set --data-dir D:\nmrj-data
python .\scripts\nmrj_cli.py config-set --attachment-dir D:\nmrj-data\attachments
python .\scripts\nmrj_cli.py config-set --backup-dir D:\nmrj-data\backups
python .\scripts\nmrj_cli.py config-set --hook-command "your-hook-command"
python .\scripts\nmrj_cli.py config-set --ai-command "your-ai-plan-command"
```

## 任务

```powershell
python .\scripts\nmrj_cli.py task-add --project nmrj --main "文档" --sub "补 README" --estimate 2
python .\scripts\nmrj_cli.py task-list --project nmrj --status 进行中
python .\scripts\nmrj_cli.py task-get --id 2026053101
python .\scripts\nmrj_cli.py task-update --id 2026053101 --progress 80 --status 进行中
python .\scripts\nmrj_cli.py task-delete --id 2026053101 --yes
```

删除任务前应确认是否同步删除附件：

```powershell
python .\scripts\nmrj_cli.py task-delete --id 2026053101 --delete-attachments --yes
```

## 日报

```powershell
python .\scripts\nmrj_cli.py report-add --task 2026053101 --log "完成 README" --hours 2 --progress 100 --date 2026-05-31
python .\scripts\nmrj_cli.py report-list --task 2026053101
python .\scripts\nmrj_cli.py report-export --from 2026-05-01 --to 2026-05-31 --out D:\reports\may.csv --overwrite
```

## 附件

```powershell
python .\scripts\nmrj_cli.py attach-add --task 2026053101 --file D:\docs\design.md
python .\scripts\nmrj_cli.py attach-list --task 2026053101
python .\scripts\nmrj_cli.py attach-open --task 2026053101
python .\scripts\nmrj_cli.py attach-delete --task 2026053101 --name design.md --yes
```

## 数据

```powershell
python .\scripts\nmrj_cli.py data-backup
python .\scripts\nmrj_cli.py data-backup --out D:\backup\nmrj-backup.zip
python .\scripts\nmrj_cli.py data-export --out D:\backup\nmrj-export.zip
python .\scripts\nmrj_cli.py data-import --file D:\backup\nmrj-export.zip --yes
```

导入会覆盖当前数据，执行前必须确认。

## AI 排期建议

```powershell
python .\scripts\nmrj_cli.py plan-suggest --from 2026-06-01 --to 2026-06-07 --daily-hours 6 --only-active
python .\scripts\nmrj_cli.py plan-suggest --task 2026053101 --out D:\reports\plan.csv
```

## 交互模式

TUI 是长期交互会话，通常由用户自己运行：

```powershell
C:\JPZ_Workspace\AZ_TOOLS\nmrj\nmrj.exe tui
```
