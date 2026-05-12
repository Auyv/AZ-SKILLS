---
name: skill-name
description: 何时触发，做什么
version: 1.0.0

inputs:
  required:
    - name: input_name
      type: string
      description: 输入项说明
      validation: 验证规则（如必填、格式要求）
  optional:
    - name: optional_input
      type: string
      description: 可选输入项说明
      default: 默认值

outputs:
  - name: output_name
    type: string
    description: 输出项说明
    format: 输出格式要求
  - name: output_file
    type: file
    description: 输出文件说明
    path: 保存路径

errors:
  - code: ERR_001
    message: 错误信息描述
    handling: 处理方式
  - code: ERR_002
    message: 错误信息描述
    handling: 处理方式
---

# 技能名称

## 做什么
[技能的具体说明]

## 何时使用
[触发场景的描述]

## 怎么做
[具体步骤或指南]

### 输入验证

在执行主要逻辑之前，验证输入参数：

```python
def validate_inputs(inputs, required_fields):
    """验证必需输入字段"""
    missing = [f for f in required_fields if f not in inputs or inputs[f] is None]
    if missing:
        raise ValueError(f"缺少必需字段: {', '.join(missing)}")

    # 类型检查
    for field, expected_type in type_requirements.items():
        if field in inputs and inputs[field] is not None:
            if not isinstance(inputs[field], expected_type):
                raise TypeError(f"字段 {field} 类型错误，期望 {expected_type}")
```

### 异常处理

使用 try-except 包裹关键步骤，提供有意义的错误信息：

```python
try:
    result = perform_action(input_data)
except FileNotFoundError as e:
    logger.error(f"文件不存在: {e}")
    return {"status": "error", "code": "ERR_FILE_NOT_FOUND", "message": f"文件不存在: {e}"}
except PermissionError as e:
    logger.error(f"权限不足: {e}")
    return {"status": "error", "code": "ERR_PERMISSION_DENIED", "message": f"权限不足: {e}"}
except Exception as e:
    logger.error(f"未知错误: {e}")
    return {"status": "error", "code": "ERR_UNKNOWN", "message": f"发生错误: {str(e)}"}
```

### 输出格式化

所有输出必须包含标准结构：

```python
def format_output(status, data=None, error=None, code=None):
    """标准输出格式"""
    output = {
        "status": status,  # "success" | "error" | "partial"
        "timestamp": datetime.now().isoformat(),
    }
    if data is not None:
        output["data"] = data
    if error is not None:
        output["error"] = {"code": code, "message": error}
    return output
```

### 错误恢复

对于可恢复的错误，提供降级策略：

1. **降级到默认行为**：主要方法失败时使用备用方案
2. **部分完成**：尽可能返回已完成的部分
3. **用户提示**：无法处理时请求用户介入

## 异常场景

在创建技能时，必须设计以下异常场景的测试用例：

| 异常场景 | 输入 | 预期行为 |
|---------|------|---------|
| 空输入 | `{}` | 返回验证错误，提示缺少必需字段 |
| 类型错误 | `{"field": 123}` 当期望 string | 返回类型错误提示 |
| 文件不存在 | 指定路径的文件的父目录不存在 | 创建目录或返回错误 |
| 权限不足 | 写入到无权限目录 | 返回权限错误，提示正确的目录 |
| 超长输入 | 超过长度限制的字符串 | 返回长度超限提示或截断处理 |

## 技能文件结构

```
skill-name/
├── SKILL.md (必需. 给 AI Agent看的技能说明文档)
├── MANUAL.md (必需. 给手动用户看的操作指南文档)
├── scripts/
│   ├── validate_inputs.py   # 输入验证脚本
│   └── format_output.py     # 输出格式化脚本
├── docs/
│   ├── guide.md             # 用户使用指南
│   └── troubleshooting.md   # 异常处理指南
├── assets/
│   └── template.md          # 模板文件
└── evals/
    └── evals.json           # 测试用例（含异常场景）
```

## 测试用例结构

测试用例必须包含正常场景和异常场景：

```json
{
  "skill_name": "example-skill",
  "evals": [
    {
      "id": 1,
      "category": "normal",
      "prompt": "正常场景的用户输入",
      "expected_output": "期望结果",
      "criteria": ["检查点1", "检查点2"],
      "pass_score": 2
    },
    {
      "id": 2,
      "category": "error",
      "prompt": "异常场景的用户输入",
      "expected_behavior": "error",
      "expected_error_code": "ERR_001",
      "criteria": ["返回错误状态", "包含错误码", "错误信息清晰"],
      "pass_score": 3
    }
  ]
}
```

