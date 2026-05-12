# AZ-SKILLS 统一管理方案

> 跨 AI 工具的 Skill 管理与同步系统

---

## 📁 目录结构

```
AZ-SKILLS/
├── manifest.yaml       # 全局清单配置
├── sync_skills.py     # 同步脚本
├── README.md          # 说明文档
└── skills/            # Skill 源码（唯一真相来源）
    ├── az-sdd/
    ├── issue-tracker/
    ├── az-skill-creator/
    ├── auto-skill-creator/
    ├── az-pdf2md/
    ├── core-dump-analyzer/
    ├── network-debug/
    └── docker-image-build/
```

---

## 🎯 设计目标

1. **单一真相来源 (SSOT)**：所有 skill 源码统一存放在 `AZ-SKILLS/skills/`
2. **跨平台同步**：一键同步到 Hermes / Cline / Claude Code
3. **增量更新**：只同步有变更的 skill，避免重复复制
4. **状态追踪**：记录最后同步时间

---

## 🚀 快速开始

### 1. 查看状态

```bash
python3 sync_skills.py --list
```

### 2. 同步到所有平台

```bash
python3 sync_skills.py
```

### 3. 只同步到指定平台

```bash
python3 sync_skills.py --to cline      # 只同步到 Cline
python3 sync_skills.py --to hermes      # 只同步到 Hermes
python3 sync_skills.py --to claude_code # 只同步到 Claude Code
python3 sync_skills.py --to opencode    # 只同步到 OpenCode
python3 sync_skills.py --to openclaw    # 只同步到 OpenClaw
```

### 4. 全量同步

```bash
python3 sync_skills.py --full           # 同步所有 skills（忽略 manifest 中的平台配置）
```

### 5. 模拟运行（不实际复制）

```bash
python3 sync_skills.py --dry-run        # 先看看会同步什么
```

---

## ⚙️ manifest.yaml 配置说明

```yaml
skills:
  - name: az-sdd                    # Skill 名称
    description: 规范驱动开发流程    # 描述
    platforms:
      hermes: az-sdd               # Hermes 中使用的名称
      cline: az-sdd                # Cline 中使用的名称
      claude_code: az-sdd          # Claude Code 中使用的名称
      opencode: az-sdd             # OpenCode 中使用的名称

sync_targets:
  - name: hermes
    path: "C:/Users/jpz/.hermes/skills"
    enabled: true                   # 是否启用

sync_mode: "incremental"            # 同步模式: incremental / full / additive
```

---

## 📋 工作流程

### 新增一个 Skill

1. 在 `skills/` 下创建目录，如 `skills/my-new-skill/`
2. 在 `manifest.yaml` 的 `skills` 列表中添加配置
3. 运行 `python3 sync_skills.py` 同步到各平台

### 更新现有 Skill

1. 修改 `skills/<skill-name>/` 下的文件
2. 运行 `python3 sync_skills.py` 增量同步

### 停用一个 Skill

1. 在 `manifest.yaml` 中删除对应条目，或设置 `enabled: false`
2. 运行 `python3 sync_skills.py` 清理目标目录

---

## 🔧 依赖安装

```bash
pip install pyyaml
```

---

## 📝 注意事项

1. **Hermes 符号链接问题**：Hermes 使用符号链接，Windows 下脚本会自动用复制替代
2. **目录权限**：确保目标目录有写入权限
3. **Windows 路径**：脚本使用正斜杠 `/`，兼容 WSL 和 Windows

---

## 🤝 支持的平台

| 平台 | 路径 | 状态 |
|------|------|------|
| Hermes (奔霄) | `C:/Users/jpz/.hermes/skills` | ✓ |
| Cline | `C:/Users/jpz/.agents/skills` | ✓ |
| Claude Code | `C:/Users/jpz/.claude/skills` | ✓ |
| OpenCode | `C:/Users/jpz/.config/opencode/skills` | ✓ |
| OpenClaw | `C:/Users/jpz/.openclaw/skills` | ✓ |

---

**文档版本**：v1.1
**更新日期**：2026-05-08
