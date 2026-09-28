# AZ-SKILLS

跨 AI 工具的 Skill 源码、版本和同步管理仓库。普通 Skill 与工作流型 Skill 使用同一套结构：源码统一位于 `skills/`，工作流型 Skill 使用 `<name>-workflow` 命名。

## 仓库结构

```text
AZ-SKILLS/
├── manifest.yaml.demo    # 本地 manifest 示例
├── sync_skills.py        # 同步到各 AI 平台
├── rules/                # 全局规则和按需加载的领域规则
├── tests/                # 同步器和结构契约测试
└── skills/               # Skill 唯一源码来源
    ├── az-skill-creator/
    ├── az-ai-handover-workflow/
    └── <name>-workflow/
```

`manifest.yaml` 按用户本机环境配置，仅在本地保存，不提交到仓库。仓库只跟踪 `manifest.yaml.demo`，避免把某个用户的路径、平台选择或账号配置传播给其他用户。

## 首次使用

```powershell
Copy-Item manifest.yaml.demo manifest.yaml
# 按本机平台、目标路径和需要加载的技能编辑 manifest.yaml
python sync_skills.py --list
```

依赖仅在使用 YAML 解析时需要：

```powershell
pip install pyyaml
```

## 常用命令

```powershell
# 查看本地 manifest 和目标平台状态
python sync_skills.py --list

# 同步所有已启用平台
python sync_skills.py

# 同步指定平台
python sync_skills.py --to codex

# 只同步一个 Skill，可继续用 --to 限定目标平台
python sync_skills.py --skill az-skill-creator --to codex

# 模拟运行，不复制文件也不更新 last_sync
python sync_skills.py --dry-run
```

同步器优先创建链接；Windows 无法创建链接时回退为 junction 或目录复制。清理只针对指向本仓库 `skills/` 的受管链接，不删除目标平台中的用户自建 Skill。

## Skill 创建约定

### 普通 Skill

在 `skills/<name>/SKILL.md` 创建，目录名、front matter 的 `name` 和本地 manifest 引用必须一致。

### 工作流型 Skill

工作流不是独立制品，而是包含多个可复用阶段、状态门槛或交接点的 Skill。在 `skills/<name>-workflow/SKILL.md` 创建，与普通 Skill 共用发现、同步和版本管理。

- Skill 负责触发范围、阶段编排、判断、人工门禁、停止条件和交接产物。
- 已有执行能力优先由 MCP 提供；只有确有确定性解析、批处理或跨平台执行价值时才增加 Python 脚本。
- 一次性任务、未经验证的尝试和项目专属临时配置不固化为工作流型 Skill。

## 创建与发布边界

- 评估/测试：默认只读，不修改源码、manifest 或目标平台。
- 创建/优化：用户明确要求后修改 `skills/` 源码，并执行相称验证。
- 登记/同步：用户明确要求发布时，修改本地 `manifest.yaml`，先 dry-run，再按需正式同步。
- 删除、改名、迁移：先列出影响范围和回滚方式，得到明确授权后执行。

全局规则位于 `rules/AZ_GLOBAL.rules.md`。修改技能前还应读取本目录的 `AGENTS.md`。

## 支持的平台

| 平台 | 默认目标目录 | flag |
| --- | --- | --- |
| Hermes | `.hermes/skills` | `--to hermes` |
| Cline | `.agents/skills` | `--to cline` |
| Claude Code | `.claude/skills` | `--to claude_code` |
| OpenCode | `.config/opencode/skills` | `--to opencode` |
| OpenClaw | `.openclaw/skills` | `--to openclaw` |
| Codex | `.codex/skills` | `--to codex` |
| Pi | `.pi/agent/skills` | `--to pi` |

## 验证

```powershell
python -m unittest discover -s tests -v
git diff --check
python sync_skills.py --dry-run
```

`manifest.yaml` 是用户本地配置，因此完整同步结果取决于本机启用的平台和技能选择。
