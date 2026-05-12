---
name: cross-platform-skill-sync
description: |
  跨 AI 工具的 Skill 统一管理与同步方案。
  适用于：用户在多个 AI 编程助手（Hermes/Cline/Claude Code）间需要保持 skill 一致性的场景。
  核心思路：中心源存储 + Python 同步脚本，替代 Windows 不支持的 symlink。
---

# Cross-Platform Skill Sync

跨 AI 工具的 Skill 统一管理与同步。

## 背景问题

- 不同 AI 工具的 skill 存放位置不同
- Windows 文件系统不支持跨驱动器的符号链接
- 需要在多个 AI 工具间保持 skill 一致性

## 解决方案

### 架构

```
AZ-SKILLS/                    ← 唯一真相来源 (SSOT)
  ├── manifest.yaml           ← 全局清单
  ├── sync_skills.py          ← Python 同步脚本
  ├── skills/                 ← Skill 源码
  │   ├── az-sdd/
  │   ├── issue-tracker/
  │   └── ...
  └── README.md
```

### manifest.yaml 配置

```yaml
source_dir: "skills"

skills:
  - name: az-sdd
    description: 规范驱动开发流程
    platforms:
      hermes: az-sdd
      cline: az-sdd
      claude_code: az-sdd

sync_targets:
  - name: hermes
    path: "C:/Users/jpz/.hermes/skills"
    enabled: true
  - name: cline
    path: "C:/Users/jpz/.agents/skills"
    enabled: true
  - name: claude_code
    path: "C:/Users/jpz/.claude/skills"
    enabled: true

sync_mode: "incremental"
last_sync: null
```

### 同步脚本用法

```bash
python3 sync_skills.py --list          # 查看状态
python3 sync_skills.py                 # 增量同步
python3 sync_skills.py --full          # 全量同步
python3 sync_skills.py --to cline      # 只同步到 Cline
python3 sync_skills.py --dry-run       # 模拟运行
```

## 已知约束

| 平台 | Skill 路径 | 同步方式 |
|------|-----------|---------|
| Hermes (奔霄) | `~/.hermes/skills/` | 符号链接(WSL)/复制(Windows) |
| Cline | `.agents/skills/` | 复制 |
| Claude Code | `.claude/skills/` | 复制 |

## 文件位置

```
C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\
```

## 使用场景

触发条件：
- 用户提到"同步 skill"、"skill 迁移"、"统一管理 skill"
- 用户在多个 AI 工具间切换工作
- 用户创建了新 skill 需要同步到多个平台

## 依赖

```bash
pip install pyyaml
```
