# 自动技能创建器 - 手动操作指南

## 概述

`auto-skill-creator` 技能可以帮助您将满意的对话过程快速创建为可复用的技能文档。当您在对话中表达满意时，AI Agent 会询问是否需要创建技能。

## 使用前提

- 已安装 Cline 或支持技能的 AI Agent
- 技能路径：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\auto-skill-creator`

## 基本使用流程

### 1. 正常使用对话

在日常对话中，当 AI Agent 帮您解决问题后：

```
您: 帮我创建一个自动总结的技能
Agent: 好的，我来帮您...
Agent: 已完成，需要调整吗？
您: 搞定！
```

### 2. 同意创建技能

当 Agent 检测到满意信号后，会询问：

```
Agent: 看起来问题已经解决了！是否需要将这个过程创建成一个可复用的技能？
您: 好啊
```

### 3. 提供技能信息

Agent 会引导您提供：

- **技能名称**：给新技能起个名字（如 `task-summarizer`）
- **触发场景**：什么时候使用这个技能
- **核心功能**：这个技能能做什么

### 4. 查看创建的技能

技能会保存在：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\{技能名称}\`

## 配置文件管理

### 查看当前配置

配置文件位于：`config/skill-creator-config.json`

### 添加正向关键词

在对话中说：

> "把'太棒了'加到正向关键词里"

或者手动编辑配置文件，在 `positive_keywords` 数组中添加新关键词。

### 添加负向关键词

> "把'卡死了'加到负向关键词里"

### 修改技能保存路径

> "把技能保存路径改成 D:\my-skills"

或者直接修改配置文件中的 `skills_base_path`。

## FAQ 功能

对话中的负向反馈（问题/抱怨）会自动收集并生成 FAQ 文档。

FAQ 文件保存在：`{技能名称}/docs/faq/faq-{技能名称}.md`

### 查看 FAQ

在需要时，FAQ 内容会被加载到上下文中。也可以手动查看文件。

## 常见问题

### Q: 技能创建过程中可以取消吗？

A: 可以。在任何阶段说"算了"或"不要创建了"即可取消。

### Q: 创建的技能在哪里？

A: 默认在 `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\AZ-SKILLS\{技能名称}\`

### Q: 可以修改默认保存路径吗？

A: 可以。编辑 `config/skill-creator-config.json`，修改 `skills_base_path` 值。

### Q: 关键词检测是大小写敏感的吗？

A: 不是。检测时会自动转换为小写进行比较。

### Q: 如何手动触发技能创建？

A: 在对话中直接说"创建技能"或"把这个做成技能"。

## 手动运行脚本

如果您想直接运行脚本，需要 Python 环境：

```bash
# 检测关键词
python scripts/detect_keywords.py "搞定了，很好！"

# 提取对话内容
python scripts/extract_dialog.py
```

## 备份配置

建议定期备份 `config/skill-creator-config.json`，以免配置丢失。
