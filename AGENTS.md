# AGENTS.md — AZ-SKILLS

跨 AI 工具的 Skill 管理仓库。所有技能源码（包括工作流型 Skill）统一位于 `skills/`。

## 仓库结构

```
AZ-SKILLS/
├── manifest.yaml.demo    # manifest.yaml 示例；实际 manifest 仅在用户本地维护
├── sync_skills.py        # 同步到各 AI 平台
├── skills/               # 技能源码；工作流型 Skill 使用 <name>-workflow 命名
│   ├── issue-tracker/    # 每个技能：目录名 = 技能名，含 SKILL.md
│   └── ...
```

## 关键命令

```powershell
# 依赖
pip install pyyaml

# 首次使用：复制示例并按本机平台修改（manifest.yaml 不提交）
Copy-Item manifest.yaml.demo manifest.yaml

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

```

## 工作流

- **新增普通技能**: 在 `skills/<name>/` 下创建含 YAML front matter 的 `SKILL.md` → 经用户授权后更新本地 `manifest.yaml` → 运行 `sync_skills.py`
- **新增工作流型 Skill**: 在 `skills/<name>-workflow/` 下创建含 YAML front matter 的 `SKILL.md` → 经用户授权后更新本地 `manifest.yaml` → 运行 `sync_skills.py`
- **更新技能**: 直接修改对应源码目录下的文件 → 运行 `sync_skills.py`（增量同步，只复制变更的文件）

## 结构要点

- AZ-SDD 已迁移到独立仓库 `git@gitee.com:AZ-TOOLS/azSdd.git`，不再由本仓库同步。
- `sync_skills.py` 只发现 `skills/`；工作流型 Skill 与普通 Skill 使用同一套同步、加载和交接机制。
- `manifest.yaml` 仅用于当前用户的平台适配，仓库只保留 `manifest.yaml.demo`；`last_sync` 由本地同步脚本自动更新。
- 技能源码中的 `.bak`、`archive` 和临时产物不得作为技能资源提交；备份统一放在 `.ai_tmp/bak/`。

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
- 写入中间文件时使用 `./.claude_tmp` 而非 `.claude/plans/`（az-sdd 约定）。
- 生成或修改 Windows `.bat` 脚本时，必须强制使用 `CRLF` 换行符，避免 `cmd.exe` 解析异常。

## AZ-SDD 使用约定

- `az-sdd/` 是本仓库的规范驱动开发档案目录；项目背景、定位和仓库约定仍以本文件为单一来源。
- 当前状态以 `az-sdd/STATUS.md` 为准，关键历史事件记录在 `az-sdd/TRACK.md`。
- 功能规范、技术规范和反馈分别维护在 `az-sdd/spec/`、`az-sdd/arch/` 和 `az-sdd/feedback/`，并同步更新对应索引。
- 仅在用户显式调用 AZ-SDD 流程时创建或更新相关档案。
