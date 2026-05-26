# Skill 反馈收集与每日进化工具

## 文件入口

| 用途 | 文件 |
|------|------|
| 打开反馈单 | `scripts/OpenSkillFeedback.bat` |
| 每日归档入口 | `scripts/RunDailySkillEvolution.ps1` |
| 安装 Windows 计划任务 | `scripts/InstallDailySkillEvolutionTask.ps1` |
| Codex 自动化提示词 | `scripts/DailySkillEvolutionPrompt.md` |
| 表单程序 | `tools/skill_feedback/open_feedback_form.py` |
| 归档与备份程序 | `tools/skill_feedback/daily_evolution.py` |

## 数据目录

| 目录 | 说明 |
|------|------|
| `skill-feedback/inbox/` | 新提交但尚未处理的反馈单 |
| `skill-feedback/archive/YYYY-MM-DD/` | 每日归档目录，包含反馈单、工作单和汇报 |
| `skill-feedback/backups/<skill>/` | 修改 skill 前的备份，每个 skill 最多 12 次 |

## 手工使用

打开反馈单：

```powershell
scripts\OpenSkillFeedback.bat
```

查看待处理反馈：

```powershell
python tools\skill_feedback\daily_evolution.py list
```

准备每日工作单：

```powershell
python tools\skill_feedback\daily_evolution.py prepare
```

修改 skill 前备份：

```powershell
python tools\skill_feedback\daily_evolution.py backup --skill auto-skill-creator
```

## 每日自动化策略

每天 1 点自动化应读取 `scripts/DailySkillEvolutionPrompt.md` 并执行其中流程：

1. 运行 `prepare` 归档未处理反馈。
2. 阅读当日 `evolution-workorder.md`。
3. 按评价原则判断反馈。
4. 修改任何 skill 前先运行 `backup`。
5. 更新当日 `evolution-report.md`。

## 设计边界

- 表单程序只收集反馈，不直接判断或修改 skill。
- 归档脚本只做机械整理和备份，不替代 AI 判断。
- 修改动作必须引用具体反馈单，并写入当日汇报。
- 非正向反馈、职责外反馈、个人偏好型反馈，应记录为非修改点。
