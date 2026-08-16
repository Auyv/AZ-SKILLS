---
name: az-sdd-tester
version: 4.1.0
description: |
  执行 AZ-SDD 交互式人工测试验收。仅在用户显式调用 /az-sdd-tester、
  az-sdd tester、sdd tester，或明确要求按 AZ-SDD/SDD 验收时触发。
  普通“跑测试”“测试一下”使用项目原有测试方式，不创建验收工单。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 验收意图
  optional:
    - name: project_path
      type: string
      default: 当前目录
    - name: feature_id
      type: string
      default: null
    - name: change_id
      type: string
      default: null
    - name: action
      type: string
      default: create
      validation: create、continue、summarize 或 close
    - name: dry_run
      type: boolean
      default: false
outputs:
  - name: result
    type: object
    description: 测试工单、验收结果、异常、证据和下一步动作
errors:
  - code: ERR_INVALID_INPUT
    message: 验收目标或结果格式错误
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_ARTIFACT_NOT_FOUND
    message: 规范、人工测试用例或开发证据不存在
  - code: ERR_TEST_PREREQUISITE
    message: 应用无法启动、关闭或进入验收环境
  - code: ERR_TEST_FAILED
    message: 自动化测试或人工验收失败
  - code: ERR_WRITE_PERMISSION
    message: 无法写入测试工单或汇总
  - code: ERR_CHANGE_NOT_READY
    message: v4 change 缺少任务或自动化证据，不能验收
  - code: ERR_RISK_GATE_REQUIRED
    message: 风险要求完整测试或人工验收但证据不足
---

# AZ-SDD-TESTER

## 做什么

把规范或 delta 的人工测试条件变成可执行工单；v4 将结论集中写入 change 的 acceptance.md。

## 何时使用

只响应显式 AZ-SDD/SDD 验收意图。普通自动化测试请求不触发。

## 怎么做

1. validate 项目并确认目标功能处于 testing/acceptance。
2. v3 读取 spec；v4 同时读取 baseline、proposal 和 spec.delta 获取变化后的验收口径。
3. v3 读取 arch；v4 读取 change 的 design 获取运行条件。
4. 引用 coder 的近期自动化测试证据；v4 要求 evidence.md 非占位。
5. 确认应用能启动、关闭并进入验收环境；失败返回 ERR_TEST_PREREQUISITE。
6. 为每个 MT 用例创建独立测试单，包含前置条件、步骤、预期、实际、结果和证据。
7. 用户执行并填写结果；AI 不代替用户确认主观体验和实机结果。
8. 失败时分类：
   - 实现缺陷 → feedback/coder
   - 规范问题 → spec
   - 环境/架构问题 → arch/coder
9. v3 生成 test-summary.md；v4 更新 `changes/{change_id}/acceptance.md`。
10. v3 按状态机推进；v4 全部通过后把 `验收结果` 写为 `通过`，再运行 change-validate。
11. 失败时保留证据和开放 change，不允许 archive。

## 自适应验证

- 无风险快速路径：复用充分的自动化证据，记录关键 acceptance 后连续归档。
- full_test_required：必须记录完整测试范围。
- manual_acceptance_required：必须由用户或实机验收并记录验收类型。
- 简单任务减少工单数量，不省略 evidence、acceptance 或冲突检查。

## 关键约束

- arch 不能改变人工验收口径。
- 未验证通过不标记 completed。
- 默认生成 Markdown；只有用户明确要求才生成 HTML 验收页。

## 按需读取

- 完整异常处理和测试单格式：references/workflow-details-v3.md
- HTML 辅助资料：references/generate-test-review-html.py
- 人工步骤：MANUAL.md
