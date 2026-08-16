# azKits 配置适配技能

本目录提供两个独立技能包，帮助用户维护自己的 azKits YAML 配置：

| 技能 | 用途 | 脚本入口 |
|---|---|---|
| `azkits-create-config` | 创建不含工具条目的最小合法配置 | `azkits-create-config/scripts/create_config.py` |
| `azkits-add-tool` | 向已有配置添加一个结构化工具描述 | `azkits-add-tool/scripts/add_tool.py` |

推荐顺序：先用 `azkits-create-config` 创建入口，再按需多次使用 `azkits-add-tool` 添加 BAT、CMD、PowerShell、Python、Shell 或 EXE 工具。

这些目录是项目内技能草稿/源码包，便于随 azKits 一起维护和分发；当前仓库没有声明 Codex 项目技能自动加载目录，因此不会自动出现在 Codex 技能列表中。每个技能的 AI 使用规则见各自的 `SKILL.md`，无 AI 环境下的命令用法见各自的 `MANUAL.md`。
