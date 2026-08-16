---
name: az-sdd-coder
version: 4.1.0
description: |
  按已批准 AZ-SDD 规范实施代码。仅在用户显式调用 /az-sdd-coder、
  az-sdd coder、sdd coder，或明确要求按 AZ-SDD/SDD 已批准规范开发时触发。
  普通写代码、修 bug、继续开发不触发。
inputs:
  required:
    - name: user_intent
      type: string
      validation: 非空，且包含明确 AZ-SDD/SDD 编码意图
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
      default: implement
      validation: plan、implement、continue、batch 或 refactor
    - name: dry_run
      type: boolean
      default: false
outputs:
  - name: result
    type: object
    description: 开发计划、修改文件、测试证据、状态和下一步动作
errors:
  - code: ERR_INVALID_INPUT
    message: 开发目标或功能编号无效
  - code: ERR_PROJECT_NOT_INITIALIZED
    message: 项目尚未初始化 AZ-SDD
  - code: ERR_SPEC_NOT_APPROVED
    message: 缺少已检查的 proposal 或 delta
  - code: ERR_ARCH_REQUIRED
    message: 风险门禁要求 design，但设计尚未完成
  - code: ERR_TEST_FAILED
    message: 自动化测试失败，不能进入待验收
  - code: ERR_EXTERNAL_TOOL_MISSING
    message: 构建或测试工具不存在
  - code: ERR_CHANGE_NOT_READY
    message: v4 change 的 proposal、delta 或 design 尚未满足开发前置条件
  - code: ERR_RISK_GATE_REQUIRED
    message: 风险触发的设计或测试前置门禁未满足
---

# AZ-SDD-CODER

## 做什么

消费已经澄清和检查的规范或 v4 change，实施任务并把自动化验证证据回写到同一 change。

## 何时使用

只响应显式 AZ-SDD/SDD 编码意图。所有 change 必须有已检查的 proposal/delta；design_required 时必须先完成 design。

## 怎么做

1. 运行：

       python ../az-sdd-shared/scripts/az_sdd.py analyze {project_path}

2. 存在 critical 时停止；缺 spec 返回 ERR_SPEC_NOT_APPROVED，缺 arch 返回 ERR_ARCH_REQUIRED。
3. v4 先确认 proposal/delta 已完成且 baseline 无冲突；任务写入 `changes/{change_id}/tasks.md`。
4. action=plan 或首次执行时：
   - 读取 spec 的需求、验收标准和测试分类。
   - 读取 arch 的实现、运行和验证约束。
   - v3 创建 coding/{timestamp}-plan.coding.md；v4 更新 change 的 tasks.md。
   - 每项任务映射到验收编号和具体文件。
   - 多功能按依赖分批；能并行的任务明确标注。
5. 向用户展示计划和修改范围，获得批准后实施。
6. TDD 优先：先写失败测试，再实现最小代码，再重构。
7. 使用项目已有构建、测试、静态检查入口；不主动安装依赖。
8. v4 将命令、结果、修改文件和覆盖范围写入 `evidence.md`；v3 写原 coding/test 证据。
9. v3 测试通过后用 transition 脚本推进 developing → testing：

       python ../az-sdd-shared/scripts/az_sdd.py transition ID developing testing --reason "自动化测试通过"

10. v4 测试失败时保留未完成任务和失败证据，不进入 acceptance。
11. next_action 指向 az-sdd-tester。

## 批量执行

- 只选择 approved/developing 功能。
- 生成一个统一计划，按依赖批次执行并实时更新。
- 每个功能独立保存测试证据和状态，单个失败不伪装成全部成功。
- 未经用户批准不开始多文件实现。

## 关键约束

- coder 不再承担完整 spec 编写或 arch 决策。
- 不用 arch 改写用户验收口径。
- 不使用 --no-verify 等绕过手段。
- 连续失败时记录根因并调整方案。

## 按需读取

- 完整批量流程、计划模板和测试格式：references/workflow-details-v3.md
- 操作手册模板：references/operation-manual.template.md
- 测试报告模板：references/test-report.template.md
- 人工步骤：MANUAL.md
