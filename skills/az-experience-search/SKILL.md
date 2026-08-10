---
name: az-experience-search
description: |
  当用户要求排查、诊断、修复问题，询问“有没有类似问题”“先查经验库”，或指定某条经验作为参考时使用。在执行诊断前搜索经验仓库，返回可解释的相似经验和适用差异。
version: 1.0.0
inputs:
  required:
    - name: repo_path
      type: string
      description: 经验仓库本地路径。
      validation: 目录存在且包含 experiences/。
    - name: query
      type: string
      description: 当前问题、错误日志、错误码或自然语言描述。
      validation: 非空字符串，最长 20000 字符。
  optional:
    - name: reference_path
      type: string
      description: 用户指定的参考经验路径。
      default: ""
    - name: top_k
      type: integer
      description: 返回候选数量。
      default: 5
outputs:
  - name: result
    type: object
    description: 包含 status、timestamp、查询词和候选经验。
    format: JSON 风格结构
errors:
  - code: ERR_REPO_NOT_FOUND
    message: 经验仓库不存在或缺少 experiences/。
    handling: 请求正确 repo_path。
  - code: ERR_EMPTY_QUERY
    message: 查询内容为空。
    handling: 请求提供现象、日志或关键词。
  - code: ERR_QUERY_TOO_LONG
    message: 查询超过 20000 字符。
    handling: 提取关键错误片段后重试。
  - code: ERR_REFERENCE_NOT_FOUND
    message: 指定参考经验不存在。
    handling: 报告路径错误并退化为普通查询。
  - code: ERR_NO_MATCH
    message: 未找到相似经验。
    handling: 明确报告未命中，然后继续正常排查。
  - code: ERR_SEARCH_FAILED
    message: 搜索脚本或文件读取失败。
    handling: 保留原始错误并退化到 rg 手工搜索。
---

# 经验查询

## 做什么

在开始排查前查询类似经验，说明为什么匹配、哪里不同、哪些步骤值得优先验证。

## 何时使用

- 用户要求排查、诊断、修复或分析报错。
- 用户询问经验库中是否有类似问题。
- 用户指定一条历史经验作为参考。

## 怎么做

1. 读取仓库 `AGENTS.md`。
2. 从当前问题提取错误码、地址、日志关键字、平台、架构和工具名。
3. 如果提供 `reference_path`，先读取参考经验，将其标签、症状和根因加入查询。
4. 优先执行：

```powershell
python scripts/search_experiences.py --repo <repo> --query "<query>" --top-k 5
```

5. 读取排名最高的 3～5 个文件，由 AI 判断真实语义和适用边界。
6. 每条结果说明：路径、命中特征、环境差异、最后验证时间和建议动作。
7. 没有命中时明确返回 `ERR_NO_MATCH`，然后继续正常排查，不阻断任务。
8. 问题结束后触发 `az-experience-capture`：成功则补充验证，失效则补充边界，未命中则创建新经验。

## 输出示例

```json
{
  "status": "success",
  "timestamp": "ISO-8601",
  "data": {
    "query": "exit code 139",
    "matches": [
      {"path": "...md", "score": 12, "matched_terms": ["139", "SIGSEGV"]}
    ]
  }
}
```

## 文件结构

```text
az-experience-search/
├── SKILL.md
├── MANUAL.md
├── scripts/search_experiences.py
└── evals/evals.json
```
