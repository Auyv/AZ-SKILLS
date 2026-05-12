---
name: auto-skill-creator
description: 自动技能创建技能。当用户在对话中表达满意（如"good"、"搞定"、"可以了"等）时，自动询问用户是否将对话过程创建为新技能，并在用户同意后调用 az-skill-creator 创建技能。同时收集对话中的负向反馈生成 FAQ。
---

# 自动技能创建器

## 做什么

自动检测用户的满意信号，在用户满意时主动询问是否将对话过程创建为可复用的技能。如果用户同意，自动整理对话内容并调用 `az-skill-creator` 创建新的技能文档。

同时，收集对话中的负向反馈（问题/抱怨），生成独立的异常处理 FAQ 文档。

## 何时使用

1. **用户表达满意时**：检测到用户输入包含正向关键词
   - 常见正向关键词：`good`、`搞定`、`可以了`、`没问题`、`解决了`、`完美`、`👍`、`感谢`、`不错`、`很好`、`棒`、`ok`、`done`

2. **用户表达不满意时**：检测到用户输入包含负向关键词
   - 常见负向关键词：`不行`、`失败了`、`报错`、`不对`、`没用`、`垃圾`、`烂`、`错误`、`bug`

3. **用户请求创建技能时**：用户明确提到"创建技能"、"做成技能"、"总结成技能"等

## 怎么做

### 第一步：检测满意信号

当用户输入时，检测是否包含正向关键词或负向关键词：

```python
# 使用 scripts/detect_keywords.py
from scripts.detect_keywords import detect_keywords

has_positive, positive_found, has_negative, negative_found = detect_keywords(user_input)
```

### 第二步：主动询问

如果检测到正向关键词，礼貌询问用户：

> "看起来问题已经解决了！是否需要将这个过程创建成一个可复用的技能？"

如果检测到负向关键词，静默记录到 FAQ 收集列表。

### 第三步：收集信息

用户同意后，收集以下信息：

| 信息项 | 说明 | 示例 |
|--------|------|------|
| **技能名称** | 新技能的名称 | `docker-image-build` |
| **触发场景** | 何时使用这个技能 | "用户要求制作 Docker 镜像时" |
| **核心功能** | 技能能做什么 | "帮助用户快速创建 Dockerfile" |

### 第四步：提取对话内容

使用对话提取脚本获取相关内容：

```python
# 使用 scripts/extract_dialog.py
from scripts.extract_dialog import extract_dialog_segment, format_dialog_for_skill

dialog_data = extract_dialog_segment(messages, target_index)
formatted_dialog = format_dialog_for_skill(dialog_data)
```

### 第五步：调用技能创建器

使用 `/az-skill-creator` 指令启动技能创建流程：

1. 提供技能名称和描述
2. 提供提取的对话示例
3. 提供触发场景说明
4. 按照 `az-skill-creator` 的指引完成技能文档编写

### 第六步：生成 FAQ

对于收集到的负向反馈，生成 FAQ 文档：

```python
# 使用 scripts/extract_dialog.py
from scripts.extract_dialog import format_negative_for_faq, save_faq

faq_content = format_negative_for_faq(dialog_data, skill_name)
faq_path = save_faq(faq_content, skill_name, base_path)
```

在 SKILL.md 中添加 FAQ 链接（不直接嵌入内容）：

```markdown
## 相关文档

- 异常处理 FAQ：[docs/faq/faq-{skill-name}.md](docs/faq/faq-{skill-name}.md)
```

## 配置文件

技能使用配置文件管理行为：

**路径**：`config/skill-creator-config.json`

```json
{
  "skills_base_path": "C:\\JPZ_Workspace\\笔记仓库-jpz\\hq-jpz\\AZ-SKILLS",
  "positive_keywords": ["good", "搞定", "可以了", ...],
  "negative_keywords": ["不行", "失败了", "报错", ...],
  "auto_ask_enabled": true,
  "faq_save_path": "docs/faq"
}
```

### 配置项说明

| 配置项 | 类型 | 说明 |
|--------|------|------|
| `skills_base_path` | string | 技能存放的基础目录 |
| `positive_keywords` | array | 正向关键词列表（自动扩展） |
| `negative_keywords` | array | 负向关键词列表 |
| `auto_ask_enabled` | boolean | 是否自动询问创建技能 |
| `faq_save_path` | string | FAQ 文件的相对保存路径 |

### 运行时更新关键词

用户可以随时要求添加或移除关键词：

> "把'太棒了'加到正向关键词里"
> "移除'ok'这个关键词"

使用 `update_keywords()` 函数更新配置。

## 技能文件结构

```
auto-skill-creator/
├── SKILL.md           # 技能说明文档（给 AI Agent）
├── MANUAL.md          # 手动操作指南（给用户）
├── config/
│   └── skill-creator-config.json  # 配置文件
├── scripts/
│   ├── detect_keywords.py    # 关键词检测脚本
│   └── extract_dialog.py     # 对话内容提取脚本
└── docs/
    └── faq/
        └── faq-template.md   # FAQ 模板
```

## 注意事项

1. **不要主动创建**：必须等用户明确同意后才开始创建流程
2. **保护上下文**：提取对话时要注意保留关键的技术细节
3. **自动扩展关键词**：发现新的正向表达时建议用户添加到配置
4. **FAQ 按需加载**：SKILL.md 中只做链接，FAQ 内容在需要时加载

## 相关文档

- 手动操作指南：[MANUAL.md](MANUAL.md)
- 关键词参考：[docs/keywords_reference.md](docs/keywords_reference.md)
- FAQ 示例：[docs/faq/faq-template.md](docs/faq/faq-template.md)
