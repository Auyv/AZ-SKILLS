---
name: az-skill-creator
description: |
  触发场景（满足任一即使用）：
  - 用户提到"创建技能"、"建立新技能"、"写个技能"
  - 用户提到"优化技能"、"改进技能"、"调整技能"
  - 用户提到"测试技能"、"评估技能效果"

  职责：帮助用户将 SKILL 写清楚、做优秀
  边界：不涉及技能安装、部署、CI/CD 等其他工作
version: 2.0.0
inputs:
  required:
    - name: user_intent
      type: string
      description: 用户想要达成的目标
      validation: 非空
    - name: trigger_phrase
      type: string
      description: 触发该技能的关键词或短语
      validation: 非空
  optional:
    - name: existing_skill_path
      type: string
      description: 要优化的现有技能路径（优化场景）
      default: null
    - name: skills_base_path
      type: string
      description: 技能创建根目录；默认使用当前项目的 skills/ 目录
      default: "./skills"
    - name: dialog_context
      type: string
      description: 对话上下文摘要（创建场景）
      default: null
outputs:
  - name: skill_directory
    type: directory
    description: 完整的技能目录结构
    path: "{skills_base_path}/{skill_name}/"
  - name: skill_md
    type: file
    description: SKILL.md 主文档
    path: "{skill_directory}/SKILL.md"
  - name: manual_md
    type: file
    description: MANUAL.md 操作指南
    path: "{skill_directory}/MANUAL.md"
  - name: evals_json
    type: file
    description: 测试用例文件
    path: "{skill_directory}/evals/evals.json"
errors:
  - code: ERR_SKILL_EXISTS
    message: 技能目录已存在
    handling: 询问用户是覆盖还是重命名
  - code: ERR_INVALID_NAME
    message: 技能名称无效（包含非法字符）
    handling: 提示命名规范，仅允许字母、数字、连字符
  - code: ERR_TEMPLATE_MISSING
    message: 模板文件缺失
    handling: 使用内联默认模板继续创建
  - code: ERR_WRITE_PERMISSION
    message: 无写入权限
    handling: 提示检查目录权限设置
  - code: ERR_EMPTY_EVALS
    message: 测试用例为空
    handling: 自动生成默认测试用例或提示用户补充
---

# 技能创建器

一个专注于核心功能的技能创建器，可在各种 AI Agent中使用。

## 核心流程

1. **确定目标**：明确技能要做什么、大致如何做
2. **设计接口**：定义输入（inputs）、输出（outputs）、错误码（errors）
3. **撰写初稿**：编写技能的初始版本
4. **创建测试**：准备包含异常场景的测试用例（至少 5 个）
5. **运行测试**：在有技能的情况下执行测试(需要用户许可状态下)
6. **评估改进**：直接在对话中评估结果并迭代
7. **完成**：直到满意为止

---

## 创建技能

### 捕获意图

首先理解用户的意图：
1. 这个技能应该让 AI 能做什么？
2. 何时触发？（什么用户措辞/上下文）
3. **期望的输入是什么？哪些必需，哪些可选？**
4. **期望的输出是什么？格式和结构？**
5. **可能出现哪些错误？错误码和处理方式？**
6. 是否需要测试用例？

### 命名规范

**重要**：创建的技能名称**必须**以 `az-` 作为前缀，例如：
- `az-pdf2md`（正确）
- `pdf2md`（错误，缺少前缀）

除非用户明确指定了其他前缀，否则默认使用 `az-` 前缀。

### 创建位置

默认把新技能创建在当前项目的 `skills/<skill-name>/` 下；不要默认写入全局技能目录（如 `$CODEX_HOME/skills`）。

如果用户明确要求创建到全局目录或其他仓库，先复述目标路径并确认，确认后再写入。

### 访谈和研究

主动询问边缘情况、输入/输出格式、成功标准等细节。

### 设计接口

在写代码之前，先定义清楚接口：

**输入（inputs）**：
- 必需字段：名称、类型、验证规则
- 可选字段：名称、类型、默认值

**输出（outputs）**：
- 每个输出的名称、类型、格式
- 标准响应结构：status + data/error

**错误（errors）**：
- 至少定义 5 个常见错误码
- 每个错误的处理方式

### 编写 SKILL.md

1. 严格遵循模板编写技能文档 `template/SKILL.md`
2. **先填写 front matter 的 inputs/outputs/errors 定义**
3. skill的执行过程要尽量使用python脚本分步实现，放在 `scripts/` 目录下
4. 用户文档、开发文档、参考文档等放在 `docs/` 目录下
5. 过程中需要用到的模版、图片等资源也放在 `assets/` 目录下
6. documents/和assets/目录可以根据需要进行细分
7. 创建过程中可以随时向用户展示 SKILL.md 的草稿，获取反馈并迭代
8. **完成后必须创建 evals/evals.json，包含至少 5 个测试用例（含 2+ 异常场景）**
9. 完成后，同步整理手动执行的步骤到 `MANUAL.md` 中，方便用户在没有AI Agent的情况下也能手动执行


## 测试用例

撰写技能初稿后，设计正常场景和异常场景的测试用例。

保存到 `evals/evals.json`：

```json
{
  "skill_name": "example-skill",
  "version": "1.0.0",
  "evals": [
    {
      "id": 1,
      "category": "normal",
      "description": "正常场景描述",
      "prompt": "用户的任务提示",
      "expected_output": "期望结果的描述",
      "criteria": ["检查点1", "检查点2"],
      "pass_score": 2
    },
    {
      "id": 2,
      "category": "error",
      "description": "异常场景描述",
      "prompt": "触发异常的用户输入",
      "expected_behavior": "error",
      "expected_error_code": "ERR_xxx",
      "criteria": ["返回错误状态", "包含错误码", "错误信息清晰"],
      "pass_score": 3
    }
  ]
}
```

每个字段的用途：
- `category`：用例分类，`normal` 或 `error`
- `prompt`：触发技能的用户输入
- `expected_output`（normal）：期望输出的描述
- `expected_behavior`（error）：期望行为，必须是 `error`
- `expected_error_code`（error）：期望的错误码
- `criteria`：评估要点列表，用于人工或自动判断
- `pass_score`：通过该测试用例所需的最低得分（满分=criteria数量）

### 异常场景必须覆盖

1. **空输入**：`{}` → 返回验证错误
2. **类型错误**：`{"field": 123}` 当期望 string → 返回类型错误
3. **格式错误**：无效的格式（如日期、邮箱）→ 返回格式错误提示
4. **文件不存在**：引用不存在的文件 → 返回文件不存在错误
5. **权限不足**：写入无权限目录 → 返回权限错误

---

## 运行和评估

### 运行测试

对每个测试用例：
1. 启动子代理执行测试提示（有技能的情况下）
2. 如果可能，也运行无技能的基线对比
3. 收集输出结果

### 评估方式

直接在对话中向用户展示结果：
- 显示每个测试的提示
- 显示技能生成的输出
- 说明是否符合预期
- 询问用户反馈

### 获取反馈

向用户提问："这个结果怎么样？有什么需要改进的地方？"

根据反馈改进技能，然后重新测试，直到满意。

---

## 迭代循环

1. 根据反馈改进技能
2. 重新运行测试
3. 再次评估
4. 重复直到用户满意

### 终止条件

出现以下任一情况停止迭代：
1. 所有测试用例通过
2. 用户明确表示满意
3. 测试轮次达到 5 次上限（防止无限循环）

---

## 质量标准

### 初稿完成标准

技能初稿完成前必须满足：
1. ? SKILL.md 语法正确，front matter 完整
2. ? 包含完整的 inputs/required 和 inputs/optional 定义
3. ? 包含完整的 outputs 定义
4. ? 包含 errors 定义（至少 3 个常见错误码）
5. ? 包含「做什么」「何时使用」「怎么做」章节
6. ? 提供了文件结构说明
7. ? MANUAL.md 包含手动执行步骤

### 发布标准

技能发布前还应满足：
8. ? 至少包含 5 个测试用例（含 2+ 异常场景）
9. ? 测试用例经过验证
10. ? 关键步骤有错误处理和边界情况说明
11. ? 输入输出有清晰的格式说明

### 异常场景设计清单

创建技能时必须覆盖以下异常场景：

| 异常类型 | 检查项 | 验证方式 |
|---------|-------|---------|
| 输入验证 | 空输入、类型错误、格式错误 | 返回 ERR_xxx 并提示正确格式 |
| 文件操作 | 文件不存在、权限不足、路径无效 | 返回具体错误码和恢复建议 |
| 业务逻辑 | 状态不正确、前置条件不满足 | 返回错误并说明前置条件 |
| 资源限制 | 超时、超长输入、超大文件 | 返回限制说明或截断处理 |
| 外部依赖 | 工具不存在、服务不可用 | 提供降级方案或替代路径 |

### 输入输出检查清单

每个技能必须明确回答：

**输入**：
- [ ] 有哪些必需输入？格式/类型/验证规则是什么？
- [ ] 有哪些可选输入？默认值是什么？
- [ ] 输入超长时如何处理？（截断/拒绝/报错）

**输出**：
- [ ] 标准输出格式是什么？（必须包含 status、timestamp）
- [ ] 成功时返回什么 data？
- [ ] 失败时返回什么 error？
- [ ] 有哪些可能的 error code？

---

## 写作指南

### 保持简洁
- SKILL.md 尽量在 500 行以内
- 使用清晰的层级结构
- 完整示例、详细说明等应放在 `docs/` 目录，避免污染上下文

### 使用祈使语气
优先使用"做这个"、"不要做那个"这样的指令。

### 解释原因
解释为什么事情是重要的，而不是只用"必须"、"禁止"。

### 提供示例
包含示例可以帮助理解预期行为。完整示例见 `docs/examples/README.md`，Agent 可在需要时主动查阅。

