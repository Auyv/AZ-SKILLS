# AZ-SKILLS

## 项目基本信息

| 字段 | 内容 |
|------|------|
| 项目名称 | AZ-SKILLS |
| 项目描述 | 跨 AI 工具的 Skill 管理与同步仓库 |
| 项目定位 | 以 `skills/` 作为单一真相来源，管理、同步和持续优化个人技能库 |
| 创建日期 | 2026-05-26 |
| 项目类型 | Python 工具仓库 |

## 项目定位

本项目用于集中管理个人 Codex/Cline/Claude Code/OpenCode/OpenClaw 等平台的技能文件，并通过同步脚本分发到目标平台。

## 目标用户

主要用户为仓库维护者本人，以及需要复用这些技能的本地 AI 编程工具。

## 技术栈

- Python 标准库
- YAML manifest
- PowerShell / Batch 启动脚本

## 目录结构说明

```
az-sdd/
├── init.md
├── track.md
├── SPEC.md
├── ARCH.md
├── FEEDBACK.md
├── spec/
├── arch/
├── test/
│   ├── cases/
│   ├── reports/
│   └── manuals/
└── feedback/
```

## 更新记录

| 日期 | 更新内容 | 更新人 |
|------|---------|--------|
| 2026-05-26 | 初始化项目档案 | Codex |
