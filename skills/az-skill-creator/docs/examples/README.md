# 技能创建示例

本文档提供技能创建的完整示例，Agent 可以在需要时查阅。

---

## 示例：创建「代码审查」技能

### Step 1 - 捕获意图

```
Q: 这个技能要让 AI 能做什么？
A: 自动审查代码，发现常见问题

Q: 何时触发？
A: 用户提到"审查代码"、"检查代码"、"code review"

Q: 期望的输入是什么？
A: 必需：code（代码文本）
    可选：language（语言，auto-detect）、strict_mode（严格模式，false）

Q: 期望的输出是什么？
A: JSON 格式的审查报告，包含问题列表、严重程度、改进建议

Q: 可能出现哪些错误？
A: ERR_EMPTY_INPUT（空代码）
   ERR_LANG_UNSUPPORTED（不支持的语言）
   ERR_TOO_LONG（代码超长）
```

### Step 2 - 设计接口

**Front Matter 定义**：

```yaml
inputs:
  required:
    - name: code
      type: string
      description: 要审查的代码文本
      validation: 非空，最多 10000 字符
  optional:
    - name: language
      type: string
      description: 代码语言，auto 为自动检测
      default: auto
    - name: strict_mode
      type: boolean
      description: 严格模式
      default: false

outputs:
  - name: report
    type: json
    description: 审查报告
    format: |
      {
        "status": "success",
        "data": {
          "issues": [...],
          "summary": {...}
        }
      }
  - name: error_report
    type: json
    description: 错误响应
    format: |
      {
        "status": "error",
        "error": {"code": "ERR_xxx", "message": "..."}
      }

errors:
  - code: ERR_EMPTY_INPUT
    message: 代码为空
    handling: 返回错误码和验证提示
  - code: ERR_LANG_UNSUPPORTED
    message: 不支持该语言
    handling: 返回支持的语言列表
  - code: ERR_TOO_LONG
    message: 代码超过长度限制
    handling: 返回最大长度提示，建议分段审查
```

### Step 3 - 编写初稿

创建 `my-skill/SKILL.md`：

```markdown
---
name: code-reviewer
description: |
  触发场景：
  - 用户提到"审查代码"、"检查代码"
  功能：自动审查代码并提供改进建议
inputs:
  required:
    - name: code
      type: string
      description: 要审查的代码文本
      validation: 非空，最多 10000 字符
  optional:
    - name: language
      type: string
      description: 代码语言
      default: auto
outputs:
  - name: report
    type: json
    description: 审查报告
errors:
  - code: ERR_EMPTY_INPUT
    message: 代码为空
    handling: 返回错误码和验证提示
---

# 代码审查器

## 做什么
自动审查代码，发现潜在问题并提供改进建议。

## 何时使用
用户请求审查代码时使用。

## 怎么做
1. 验证输入（非空、长度检查）
2. 检测语言
3. 执行审查规则
4. 返回结构化报告
```

### Step 4 - 创建测试（含异常场景）

`my-skill/evals/evals.json`：

```json
{
  "skill_name": "code-reviewer",
  "version": "1.0.0",
  "evals": [
    {
      "id": 1,
      "category": "normal",
      "description": "正常审查 JavaScript 代码",
      "prompt": "审查这段代码：function test() { return 1 }",
      "expected_output": "识别出函数命名问题，提供改进建议",
      "criteria": ["识别语言为 JS", "发现问题", "提供建议"],
      "pass_score": 3
    },
    {
      "id": 2,
      "category": "error",
      "description": "空输入",
      "prompt": "审查代码：",
      "expected_behavior": "error",
      "expected_error_code": "ERR_EMPTY_INPUT",
      "criteria": ["返回错误状态", "错误码为 ERR_EMPTY_INPUT", "包含提示信息"],
      "pass_score": 3
    },
    {
      "id": 3,
      "category": "error",
      "description": "代码超长",
      "prompt": "审查代码：" + "x".repeat(15000),
      "expected_behavior": "error",
      "expected_error_code": "ERR_TOO_LONG",
      "criteria": ["返回错误状态", "错误码为 ERR_TOO_LONG", "提示最大长度"],
      "pass_score": 3
    },
    {
      "id": 4,
      "category": "normal",
      "description": "指定语言",
      "prompt": "用严格模式审查这段 Python 代码：def foo(): pass",
      "expected_output": "按 Python 规则审查，严格模式输出更多警告",
      "criteria": ["识别为 Python", "严格模式生效", "返回结构化报告"],
      "pass_score": 3
    }
  ]
}
```

### Step 5 - 运行测试

使用子代理执行测试，收集结果。

### Step 6 - 评估改进

向用户展示结果，根据反馈迭代完善。

---

## 异常场景检查清单

创建每个技能时，必须确认已覆盖：

| 异常类型 | 测试场景 | 验证点 |
|---------|---------|-------|
| 空输入 | `""`、`null`、不传参 | 返回 ERR_xxx，提示必填 |
| 类型错误 | 传 number 当 string | 返回类型错误 |
| 格式错误 | 无效邮箱/日期/URL | 返回格式错误提示 |
| 超长输入 | 超过 10000 字符 | 返回 ERR_TOO_LONG |
| 文件不存在 | 引用不存在的文件 | 返回 ERR_FILE_NOT_FOUND |
| 权限不足 | 写入只读目录 | 返回 ERR_PERMISSION_DENIED |
| 工具不存在 | 依赖的命令不存在 | 返回 ERR_TOOL_NOT_FOUND |
| 超时 | 操作超过 30 秒 | 返回 ERR_TIMEOUT |

---

## 更多示例

（待添加）
