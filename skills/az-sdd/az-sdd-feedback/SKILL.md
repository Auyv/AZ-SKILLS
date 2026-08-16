---
name: az-sdd-feedback
version: 4.1.0
description: |
  编排 AZ-SDD 反馈闭环。仅在用户显式调用 /az-sdd-feedback、
  az-sdd feedback、sdd feedback，或明确要求把反馈纳入 AZ-SDD/SDD 闭环时触发。
  普通 bug 修复、建议讨论和新需求沟通不自动创建反馈单。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确纳入 AZ-SDD/SDD 反馈闭环的意图
  optional:
    - name: project_path
      type: string
      default: 当前目录
    - name: feedback_id
      type: string
      default: 自动分配
    - name: change_id
      type: string
      default: null
    - name: action
      type: string
      default: create
      validation: create、clarify、plan、process、verify、close 或 resume
    - name: dry_run
      type: boolean
      default: false
outputs:
  - name: result
    type: object
    description: 反馈编号、分类、状态、处理计划、证据和下一步动作
errors:
  - code: ERR_INVALID_INPUT
    message: 反馈内容为空、格式错误或没有明确闭环意图
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_ARTIFACT_NOT_FOUND
    message: 反馈单或关联规范不存在
  - code: ERR_ARTIFACT_CONFLICT
    message: 反馈结论与规范或测试证据冲突
  - code: ERR_TEST_FAILED
    message: 验证失败，反馈不能关闭
  - code: ERR_WRITE_PERMISSION
    message: 无法写入反馈单或索引
  - code: ERR_CHANGE_NOT_FOUND
    message: 指定关联 change 不存在
---

# AZ-SDD-FEEDBACK

## 做什么

负责反馈从原始描述到澄清、分类、计划、处理、验证和关闭的完整闭环。

## 何时使用

只在用户明确要求纳入 AZ-SDD/SDD 反馈闭环时使用。一旦进入，本技能是闭环责任主体，其他技能作为专业阶段被调用。

## 怎么做

1. validate 项目；保存用户原始描述。
2. 创建或读取 feedback/FB-{YEAR}-{SEQ}.feedback.md；v4 若已归属 change，同时更新其 feedback.md 摘要。
3. 信息不足时自行复现或一次询问 1-3 个关键问题。
4. 分类并选择路径：
   - 实现缺陷 → coder → tester
   - 验收异常 → tester → coder → tester
   - 需求变更/新功能 → 创建/关联 change → spec → coder → tester
   - 架构/重构 → arch 或 coder → tester
   - 无法复现/不处理 → 记录缺口或关闭原因
5. 执行前写入处理计划、修改范围、验证方式和关闭条件。
6. 保持状态：待澄清、已计划、处理中、待验证、已解决、已关闭、待复现、转需求。
7. 验证失败保持处理中，返回 ERR_TEST_FAILED。
8. 只有验证通过、用户确认、明确关闭或待复现挂起时才结束。
9. 更新 FEEDBACK.md、STATUS.md，必要时追加 TRACK.md。
10. v4 反馈关闭不等于 change 自动归档；仍需完整门禁。
11. next 未完成前继续提示本反馈。

## 关键约束

- 不用摘要替换用户原始反馈。
- “已解决”必须有测试或用户确认。
- “已关闭”必须有关闭原因。
- 转需求必须关联新/改 spec。

## 按需读取

- 模板：references/feedback.template.md、references/FEEDBACK.md.template
- 完整分流与输出格式：references/workflow-details-v3.md
- 人工步骤：MANUAL.md
