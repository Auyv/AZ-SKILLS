# AGENTS.md — AZ-SKILLS

跨 AI 工具的 Skill 管理仓库。单一真相来源: `skills/` 目录。

## 仓库结构

```
AZ-SKILLS/
├── manifest.yaml         # 中心注册表：技能列表 + 平台目标路径
├── sync_skills.py        # 同步到各 AI 平台
├── manifest_update.py    # 从 SKILL.md front matter 自动发现并追加到 manifest
├── skills/               # 所有技能源码（唯一真相来源）
│   ├── az-sdd.v3/        # SDD 技能族容器（含 9 个子技能，无顶层 SKILL.md）
│   ├── issue-tracker/    # 每个技能：目录名 = 技能名，含 SKILL.md
│   └── ...
```

## 关键命令

```powershell
# 依赖
pip install pyyaml

# 查看当前同步状态
python sync_skills.py --list

# 同步到所有已启用平台
python sync_skills.py

# 同步到指定平台
python sync_skills.py --to cline

# 全量同步（忽略 manifest 平台过滤）
python sync_skills.py --full

# 模拟运行
python sync_skills.py --dry-run

# 将新技能自动添加到 manifest.yaml
python manifest_update.py --auto
```

## 工作流

- **新增技能**: 在 `skills/<name>/` 下创建含 YAML front matter 的 `SKILL.md` → 运行 `manifest_update.py --auto` → 运行 `sync_skills.py`
- **更新技能**: 直接修改 `skills/<name>/` 下的文件 → 运行 `sync_skills.py`（增量同步，只复制变更的文件）

## 结构要点

- `az-sdd.v3/` 是技能族容器，无顶层 SKILL.md，不作为独立技能同步。其下的 9 个子目录才是独立技能（如 `az-sdd-coding`, `az-sdd-test` 等）。
- 子技能目录如 `hq_tester_skills/hq_adb_srtget/` 也能被 `manifest_update.py` 正确发现（递归扫描 SKILL.md）。
- 没有 .gitignore。`manifest.yaml` 的 `last_sync` 字段会被 `sync_skills.py` 自动更新。

## 平台目标

| 平台 | 目标路径 | 命令行 flag |
|------|----------|-------------|
| Hermes | `.hermes/skills` | `--to hermes` |
| Cline | `.agents/skills` | `--to cline` |
| Claude Code | `.claude/skills` | `--to claude_code` |
| OpenCode | `.config/opencode/skills` | `--to opencode` |
| OpenClaw | `.openclaw/skills` | `--to openclaw` |

## 约定

- 每个 SKILL.md 必须有 YAML front matter 包含 `name`、`description`、`version`。
- 同步基于 mtime 比较，只在源文件较新时复制。`--full` 跳过增量检查。
- `manifest_update.py` 跳过 `template/`、`templates/` 目录及名称为 `skill-name` 的技能（模板占位符）。
- 写入中间文件时使用 `./.claude_tmp` 而非 `.claude/plans/`（az-sdd 约定）。
- 生成或修改 Windows `.bat` 脚本时，必须强制使用 `CRLF` 换行符，避免 `cmd.exe` 解析异常。

## AZ-SDD 使用约定

- `az-sdd/` 是本仓库的规范驱动开发档案目录；项目背景、定位和仓库约定仍以本文件为单一来源。
- 当前状态以 `az-sdd/STATUS.md` 为准，关键历史事件记录在 `az-sdd/TRACK.md`。
- 功能规范、技术规范和反馈分别维护在 `az-sdd/spec/`、`az-sdd/arch/` 和 `az-sdd/feedback/`，并同步更新对应索引。
- 仅在用户显式调用 AZ-SDD 流程时创建或更新相关档案。
